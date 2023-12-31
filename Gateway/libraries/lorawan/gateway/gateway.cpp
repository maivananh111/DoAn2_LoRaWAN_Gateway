/*
 * gateway.cpp
 *
 *  Created on: Nov 13, 2023
 *      Author: anh
 */

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

#include "log/log.h"
#include "sysinfo/sysinfo.h"

#include "lorawan/lrphys/lrphys.h"
#include "lorawan/lrmac/lrmac.h"
#include "lorawan/gateway/gateway.h"
#include "lorawan/base64/base64.h"

#include "lwipopts.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>
#include "lwip/apps/sntp_opts.h"
#include "lwip/apps/sntp.h"





typedef struct{
	uint8_t              channel = 0;
	bool 				 immediately = false;
	uint32_t             timestamp = 0;
	uint32_t 			 txdelay = 0;
	lrmac_phys_setting_t *setting;
	lrmac_packet_t       *packet;
} schedule_item_t;

static const char *TAG = "LoRaWAN";

static QueueHandle_t queue_rxpkt;
static QueueHandle_t queue_txpkt;
static QueueHandle_t queue_sched;

static TaskHandle_t htask_forward_uplink = NULL;
static TaskHandle_t htask_handle_downlink = NULL;
static TaskHandle_t htask_keepalive = NULL;
static TaskHandle_t htask_send_status = NULL;
static TaskHandle_t htask_schedule_downlink = NULL;


/**
 * Function prototype.
 */
static void lrwgw_handle_rxpkt(lorawan_gateway_t *pgtw);
static void lrwgw_handle_txpkt(lorawan_gateway_t *pgtw);

static void lrwgw_udpsemtech_event_handler(udpsem_t *phander, udpsem_event_t event, void *param);

static void lrwgw_task_forward_uplink(void *pgtw);
static void lrwgw_task_handle_downlink(void *pgtw);
static void lrwgw_task_schedule_downlink(void *pgtw);
static void lrwgw_task_keepalive(void *pgtw);
static void lrwgw_task_send_status(void *pgtw);


/**
 * Function declaration.
 */
void lorawan_gateway_initialize(lorawan_gateway_t *pgtw){
	queue_rxpkt = xQueueCreate(LRWGW_PHYS_RXPKT_QUEUE_SIZE, sizeof(uint32_t));
	queue_txpkt = xQueueCreate(LRWGW_PHYS_TXPKT_QUEUE_SIZE, sizeof(uint32_t));
	queue_sched = xQueueCreate(LRWGW_PHYS_TXPKT_QUEUE_SIZE, sizeof(schedule_item_t *));

	lrmac_initialize(&queue_rxpkt);

	udpsem_initialize(&pgtw->udpsemtech, &pgtw->server_info, &pgtw->gateway_info, &queue_txpkt);
	udpsem_register_event_handler(&pgtw->udpsemtech, lrwgw_udpsemtech_event_handler, NULL);
}

void lorawan_gateway_register_event_handler(lorawan_gateway_t *pgtw,
		void (*event_handler_function)(lorawan_gateway_t *pgtw, lorawan_gateway_event_t event, void *param),
		void *param){
	pgtw->event_handler = event_handler_function;
	pgtw->event_parameter = param;
}

void lorawan_gateway_set_identify(lorawan_gateway_t *pgtw, uint64_t id){
	pgtw->gateway_info.id = id;
}

void lorawan_gateway_set_coordinate(lorawan_gateway_t *pgtw, float latitude, float longitude, int altitude){
	pgtw->gateway_info.latitude  = latitude;
	pgtw->gateway_info.longitude = longitude;
	pgtw->gateway_info.altitude  = altitude;
}





err_t lorawan_gateway_start(lorawan_gateway_t *pgtw){
	err_t ret = udpsem_connect(&pgtw->udpsemtech);
	if(ret != ERR_OK) return ret;

	if(pgtw->event_handler)
		pgtw->event_handler(pgtw, LORAWAN_GATEWAY_CONNECT, pgtw->event_parameter);

	udpsem_keepalive(&pgtw->udpsemtech);

	if(htask_send_status != NULL)       vTaskResume(htask_send_status);
	else xTaskCreate(lrwgw_task_send_status,       "lrwgw_task_send_status",      4096/4,  (void *)pgtw, 4,  &htask_send_status);

	if(htask_keepalive   != NULL)       vTaskResume(htask_keepalive);
	else xTaskCreate(lrwgw_task_keepalive,         "lrwgw_task_keepalive",        4096/4,  (void *)pgtw, 2,  &htask_keepalive);

	if(htask_forward_uplink != NULL)    vTaskResume(htask_forward_uplink);
	else xTaskCreate(lrwgw_task_forward_uplink,    "lrwgw_task_forward_uplink",   10240/4, (void *)pgtw, 6,  &htask_forward_uplink);

	if(htask_handle_downlink != NULL)   vTaskResume(htask_handle_downlink);
	else xTaskCreate(lrwgw_task_handle_downlink,   "lrwgw_task_handle_downlink",  20480/4, (void *)pgtw, 10, &htask_handle_downlink);

	if(htask_schedule_downlink != NULL) vTaskResume(htask_schedule_downlink);
	else xTaskCreate(lrwgw_task_schedule_downlink, "lrwgw_task_forward_downlink", 4096/4,  (void *)pgtw, 8,  &htask_schedule_downlink);

	return ret;
}

void lorawan_gateway_stop(lorawan_gateway_t *pgtw){
	if(htask_send_status != NULL)       vTaskSuspend(htask_send_status);
	if(htask_keepalive != NULL) 		vTaskSuspend(htask_keepalive);
	if(htask_forward_uplink != NULL) 	vTaskSuspend(htask_forward_uplink);
	if(htask_handle_downlink != NULL)   vTaskSuspend(htask_handle_downlink);
	if(htask_schedule_downlink != NULL) vTaskSuspend(htask_schedule_downlink);

	udpsem_disconnect(&pgtw->udpsemtech);

	if(pgtw->event_handler)
		pgtw->event_handler(pgtw, LORAWAN_GATEWAY_DISCONNECT, pgtw->event_parameter);

	if(queue_rxpkt != NULL) vQueueDelete(queue_rxpkt);
	if(queue_txpkt != NULL) vQueueDelete(queue_txpkt);
	if(queue_sched != NULL) vQueueDelete(queue_sched);
}




static void lrwgw_handle_rxpkt(lorawan_gateway_t *pgtw){
	lrmac_packet_t *macpkt;

	if(xQueueReceive(queue_rxpkt, (void *)&macpkt, 10) == pdTRUE){
		if(macpkt != NULL){
			/** Increment rx packet counter */
			switch(macpkt->eventid){
				case LRPHYS_TRANSMIT_COMPLETED:
					pgtw->udpsemtech.rxfw++;
				break;
				case LRPHYS_RECEIVE_COMPLETED:
					pgtw->udpsemtech.rxnb++;
					pgtw->udpsemtech.rxok++;
				break;
				case LRPHYS_ERROR_CRC:
					pgtw->udpsemtech.rxnb++;
					if(macpkt != NULL) free((lrmac_packet_t *)macpkt);
					return;
				break;
			}

			/** Send rx packet to server */
			if(macpkt->payload != NULL){
				udpsem_rxpk_t rxpkt;
				lrmac_phys_info_t phys_info;

				lrmac_get_phys_info(macpkt->channel, &phys_info);

				rxpkt.channel  = macpkt->channel;
				rxpkt.rf_chain = 0;
				rxpkt.freq     = (float)(phys_info.freq / 1000000.0);
				rxpkt.crc_stat = 1;
				rxpkt.sf       = phys_info.sf;
				rxpkt.bw       = (phys_info.bw / 1000);
				rxpkt.codr 	   = phys_info.cdr;
				rxpkt.rssi 	   = phys_info.rssi;
				rxpkt.snr      = phys_info.snr;
				rxpkt.data     = (uint8_t *)macpkt->payload;
				rxpkt.size     = macpkt->payload_size;

				udpsem_push_data(&pgtw->udpsemtech, &rxpkt, 0);

				if(macpkt->payload != NULL) free(macpkt->payload);
			}

			if(pgtw->event_handler)
				pgtw->event_handler(pgtw, (lorawan_gateway_event_t)(macpkt->eventid + 2), pgtw->event_parameter);

			if(macpkt != NULL) free((lrmac_packet_t *)macpkt);
		}
		else{
			LOG_ERROR(TAG, "NULL packet from MAC");
		}
	}
}

static void lrwgw_handle_txpkt(lorawan_gateway_t *pgtw){
	uint8_t *downlink_pkt = NULL;

	if(xQueueReceive(queue_txpkt, &downlink_pkt, 10) == pdTRUE){
		udpsem_txpk_t *txpkt = NULL;
		udpsem_txpk_ack_error_t ack_error = UDPSEM_ERROR_NONE;
		uint8_t channel = 0;
		uint32_t gps_time = 0;

		if(downlink_pkt == NULL){
			LOG_ERROR(TAG, "NULL pointer at %s -> %d", __FUNCTION__, __LINE__);
			return;
		}

		txpkt = (udpsem_txpk_t *)malloc(sizeof(udpsem_txpk_t));
		if(txpkt == NULL){
    		LOG_ERROR(TAG, "Memory exhausted, malloc fail at %s -> %d", __FUNCTION__, __LINE__);
    		if(downlink_pkt != NULL) free(downlink_pkt);
    		return;
		}

		char *jsondata = (char *)(downlink_pkt + 4U);
		if(udpsem_parse_pull_resp(jsondata, txpkt) == false){
    		LOG_ERROR(TAG, "Json format error at %s -> %d", __FUNCTION__, __LINE__);
			if(txpkt->modu != NULL)  free(txpkt->modu);
			if(txpkt->data != NULL)  free(txpkt->data);
			if(downlink_pkt != NULL) free(downlink_pkt);
			if(txpkt != NULL)        free(txpkt);
    		return;
		}


		gps_time  = udpsem_get_time_stamp();
		ack_error = udpsem_check_error(txpkt, gps_time);
		channel   = lrmac_get_channel_by_freq((long)(txpkt->freq * 1E6));


		LOG_INFO(TAG, "Time tmst       : %lu",     txpkt->tmst);
		LOG_INFO(TAG, "Channel         : %d",      channel);
		LOG_INFO(TAG, "Modulation      : %s",      txpkt->modu);
		LOG_INFO(TAG, "Frequency       : %.1fMHz", txpkt->freq);
		LOG_INFO(TAG, "Spreading Factor: %d",      txpkt->sf);
		LOG_INFO(TAG, "Band Width      : %dKHz",   txpkt->bw);
		LOG_INFO(TAG, "Coding Rate     : 4/%d",    txpkt->codr);
		LOG_INFO(TAG, "Preamble length : %d",      txpkt->prea);
		LOG_INFO(TAG, "Power           : %d",      txpkt->powe);

		if(ack_error != UDPSEM_ERROR_TX_FREQ && ack_error != UDPSEM_ERROR_TX_POWER){
			lrmac_phys_setting_t *phys_setting  = (lrmac_phys_setting_t *)malloc(sizeof(lrmac_phys_setting_t));
			lrmac_packet_t       *sendpacket    = (lrmac_packet_t *)malloc(sizeof(lrmac_packet_t));
			schedule_item_t      *schedule_item = (schedule_item_t *)malloc(sizeof(schedule_item_t));

			if(phys_setting == NULL || sendpacket == NULL || schedule_item == NULL){
	    		LOG_ERROR(TAG, "Memory exhausted, malloc fail at %s -> %d", __FUNCTION__, __LINE__);
	    		if(downlink_pkt != NULL) free(downlink_pkt);
	    		if(txpkt != NULL) free(txpkt);
	    		return;
			}

			phys_setting->freq = (long)(txpkt->freq * 1E6);
			phys_setting->powe = txpkt->powe;
			phys_setting->sf   = txpkt->sf;
			phys_setting->bw   = txpkt->bw;
			phys_setting->codr = txpkt->codr;
			phys_setting->prea = txpkt->prea;
			phys_setting->crc  = txpkt->ncrc;
			phys_setting->iiq  = txpkt->ipol;

			sendpacket->channel = channel;
			sendpacket->payload = (uint8_t *)malloc(400);
			memset(sendpacket->payload, 0, 400);
			sendpacket->payload_size = b64_to_bin((const char *)txpkt->data, strlen((const char *)txpkt->data), sendpacket->payload, txpkt->size);

			schedule_item->channel     = channel;
			schedule_item->immediately = txpkt->imme;
			schedule_item->timestamp   = pgtw->udpsemtech.time_stamp;
			schedule_item->txdelay     = txpkt->tmst - pgtw->udpsemtech.time_stamp;
			schedule_item->packet      = sendpacket;
			schedule_item->setting     = phys_setting;

			if(xQueueSend(queue_sched, (void *)&schedule_item, 10) == pdFALSE){
				LOG_ERROR(TAG, "Error queue full at %s -> %d", __FUNCTION__, __LINE__);

				if(sendpacket->payload != NULL) free(sendpacket->payload);
				if(sendpacket != NULL)          free(sendpacket);
				if(phys_setting != NULL)        free(phys_setting);
				if(schedule_item != NULL)       free(schedule_item);

				if(txpkt->modu != NULL)  free(txpkt->modu);
				if(txpkt->data != NULL)  free(txpkt->data);
				if(downlink_pkt != NULL) free(downlink_pkt);
				if(txpkt != NULL)        free(txpkt);

				return;
			}
		}

		udpsem_send_tx_ack(&pgtw->udpsemtech, ack_error);
		if(txpkt->modu != NULL)  free(txpkt->modu);
		if(txpkt->data != NULL)  free(txpkt->data);
		if(txpkt != NULL)        free(txpkt);
		if(downlink_pkt != NULL) free(downlink_pkt);
	}
}




static void lrwgw_udpsemtech_event_handler(udpsem_t *pudp, udpsem_event_t event, void *param){
	switch(event.eventid){
		case UDPSEM_EVENTID_SENT_STATE:
			LOG_EVENT(TAG, "Sent gateway status");
		break;
		case UDPSEM_EVENTID_SENT_DATA:
			LOG_EVENT(TAG, "Sent uplink message");
		break;
		case UDPSEM_EVENTID_RECV_DATA:
			LOG_EVENT(TAG, "Received downlink message, token = %d", event.token);
		break;
		case UDPSEM_EVENTID_KEEPALIVE:
			LOG_EVENT(TAG, "Keep alive connection, free heap = %lubytes", dev_get_free_heap_size());
		break;
		case UDPSEM_EVENTID_SENT_ACK:
			LOG_EVENT(TAG, "Sent txpk_ack, token = %d", event.token);
		break;
		default:
		break;
	};
}



/**
 * Gateway task: lrwgw_task_forward_uplink.
 * To Do: Forward uplink message from end device to server.
 */
static void lrwgw_task_forward_uplink(void *pgtw){
	lorawan_gateway_t *gateway = (lorawan_gateway_t *)pgtw;

	while(1){
		/**
		 * Process the rxpk form lora MAC.
		 */
		lrwgw_handle_rxpkt(gateway);
	}
}

/**
 * Gateway task: lrwgw_task_forward_downlink.
 * To Do: Forward downlink message from server end device.
 */
static void lrwgw_task_handle_downlink(void *pgtw){
	lorawan_gateway_t *gateway = (lorawan_gateway_t *)pgtw;

	while(1){
		/**
		 * Process the txpk form udp semtech event.
		 */
		lrwgw_handle_txpkt(gateway);
	}
}

/**
 * Gateway task: lrwgw_task_schedule_downlink.
 * To Do: Schedule and forward downlink message.
 */
static void lrwgw_task_schedule_downlink(void *pgtw){
	lorawan_gateway_t *gateway = (lorawan_gateway_t *)pgtw;
	schedule_item_t *item = NULL;
	(void)gateway;

	while(1){
		if(xQueueReceive(queue_sched, &item, 100) == pdTRUE){
			if(item->immediately == true){ /** forward immediately */
//				LOG_WARN(TAG, "Forward down link immediately");

				lrmac_apply_setting(item->channel, item->setting);
				lrmac_send_packet(item->packet);
				lrmac_restore_default_setting(item->channel);

				if(item->setting != NULL) 		  free(item->setting);
				if(item->packet->payload != NULL) free(item->packet->payload);
				if(item->packet != NULL) 		  free(item->packet);
				if(item != NULL) 				  free(item);
			}
			else{ /** Schedule down link*/
				uint32_t delta_t = 0;

				if(udpsem_get_time_stamp() < item->timestamp)
					delta_t = udpsem_get_time_stamp() + (__UINT32_MAX__ - item->timestamp);
				else
					delta_t = udpsem_get_time_stamp() - item->timestamp;

				if(delta_t > item->txdelay){
//					LOG_WARN(TAG, "Forward down link with schedule, tx delay time = %luus", item->txdelay);

//					lrmac_apply_setting(item->channel, item->setting);
					lrmac_send_packet(item->packet);
//					lrmac_restore_default_setting(item->channel);

					if(item->setting != NULL) 		  free(item->setting);
					if(item->packet->payload != NULL) free(item->packet->payload);
					if(item->packet != NULL) 		  free(item->packet);
					if(item != NULL) 				  free(item);
				}
				else{
					xQueueSend(queue_sched, &item, 10);
				}
			}
		}
	}
}

/**
 * Gateway task: lrwgw_task_keepalive.
 * To Do: Keep alive connection with server.
 */
static void lrwgw_task_keepalive(void *pgtw){
	lorawan_gateway_t *gateway = (lorawan_gateway_t *)pgtw;

	while(1){
		/**
		 * Send pull request to keep connection.
		 */
		udpsem_keepalive(&gateway->udpsemtech);

		/**
		 * Keep alive interval, allow switch task to avoid watchdog reset.
		 */
		vTaskDelay(gateway->keepalive_interval * 1000UL);
	}
}

/**
 * Gateway task: lrwgw_task_send_status.
 * To Do: Send gateway status message to server.
 */
static void lrwgw_task_send_status(void *pgtw){
	lorawan_gateway_t *gateway = (lorawan_gateway_t *)pgtw;

	while(1){
		/**
		 * Send status interval, allow switch task to avoid watchdog reset.
		 */
		vTaskDelay(gateway->stat_interval * 1000UL);

		/**
		 * Send gateway status.
		 */
		udpsem_send_stat(&gateway->udpsemtech);
	}
}




