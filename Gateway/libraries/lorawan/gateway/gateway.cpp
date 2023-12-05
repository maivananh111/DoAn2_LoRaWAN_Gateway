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

#include "json/json.hpp"

#include "lwipopts.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>
#include "lwip/apps/sntp_opts.h"
#include "lwip/apps/sntp.h"


using json = nlohmann::json;

static const char *TAG = "LoRaWAN";

__IO QueueHandle_t queue_uplink;
__IO QueueHandle_t queue_downlink;

/**
 * Function prototype.
 */
static void lrwgw_send_status(lorawan_gateway_t *pgtw);
static void lrwgw_keepalive(lorawan_gateway_t *pgtw);
static void lrwgw_forward_upllink(lorawan_gateway_t *pgtw);
static void lrwgw_handle_udp_txpkt(lorawan_gateway_t *pgtw);

static void lrwgw_udpsemtech_event_handler(udpsem_t *phander, udpsem_event_t event, void *param);


/**
 * Function declaration.
 */


void lorawan_gateway_initialize(lorawan_gateway_t *pgtw){
	queue_uplink = xQueueCreate(LRWGW_PHYS_RXPKT_QUEUE_SIZE, sizeof(uint32_t));
	queue_downlink = xQueueCreate(LRWGW_PHYS_TXPKT_QUEUE_SIZE, sizeof(uint32_t));

	lrmac_initialize((QueueHandle_t *)&queue_uplink);

	udpsem_initialize(&pgtw->udpsemtech, &pgtw->server_info, &pgtw->gateway_info, (QueueHandle_t *)&queue_downlink);
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
    /** Init time */
	pgtw->lstat = HAL_GetTick();
	pgtw->lpull = HAL_GetTick();

	udpsem_keepalive(&pgtw->udpsemtech);

	return ret;
}

void lorawan_gateway_stop(lorawan_gateway_t *pgtw){
	udpsem_disconnect(&pgtw->udpsemtech);
}



void lorawan_gateway_process(lorawan_gateway_t *pgtw){
	/**
	 * Send gateway status.
	 */
	lrwgw_send_status(pgtw);

	/**
	 * Send pull request to keep connection.
	 */
	lrwgw_keepalive(pgtw);

	/**
	 * Forward uplink packe.
	 */
	lrwgw_forward_upllink(pgtw);

	/**
	 * Process the txpk form udp semtech event.
	 */
	lrwgw_handle_udp_txpkt(pgtw);

	/**
	 * Delay for ignore watchdog timer.
	 */
	vTaskDelay(1);
}






/** Static function */
static void lrwgw_send_status(lorawan_gateway_t *pgtw){
	if((HAL_GetTick() - pgtw->lstat) > (LRWGW_STAT_INTERVAL*1000UL)){
		udpsem_send_stat(&pgtw->udpsemtech);
		pgtw->lstat = HAL_GetTick();
	}
}

static void lrwgw_keepalive(lorawan_gateway_t *pgtw){
	if((HAL_GetTick() - pgtw->lpull) > (LRWGW_KEEP_ALIVE*1000UL)){
		udpsem_keepalive(&pgtw->udpsemtech);
		pgtw->lpull = HAL_GetTick();
	}
}

void lrwgw_forward_upllink(lorawan_gateway_t *pgtw){
	__IO lrmac_macpkt_t *macpkt;

	if(xQueueReceive(queue_uplink, (void *)&macpkt, 10) == pdTRUE){
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

				free(macpkt->payload);
				free((lrmac_macpkt_t *)macpkt);

				if(pgtw->event_handler)
					pgtw->event_handler(pgtw, LORAWAN_GATEWAY_EVENT_UPLINK, pgtw->event_parameter);
			}
		}
		else{
			LOG_ERROR(TAG, "NULL packet from MAC");
		}
	}
}

static void lrwgw_handle_udp_txpkt(lorawan_gateway_t *pgtw){
	uint8_t *downlink_pkt = NULL;

	if(xQueueReceive(queue_downlink, &downlink_pkt, 10) == pdTRUE){
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
    		free(downlink_pkt);
    		return;
		}

		if(udpsem_parse_pull_resp(downlink_pkt, txpkt) == false){
    		LOG_ERROR(TAG, "Json format error at %s -> %d", __FUNCTION__, __LINE__);
    		free(downlink_pkt);
    		return;
		}

		gps_time = udpsem_get_ntp_gps_time();

		LOG_MEM(TAG, "Modulation      : %s", txpkt->modu);
		LOG_MEM(TAG, "Frequency       : %f", txpkt->freq);
		LOG_MEM(TAG, "Spreading Factor: %d", txpkt->sf);
		LOG_MEM(TAG, "Band Width      : %d", txpkt->bw);
		LOG_MEM(TAG, "Coding Rate     : %d", txpkt->codr);
		LOG_MEM(TAG, "Preamble length : %d", txpkt->prea);
		LOG_MEM(TAG, "Power           : %d", txpkt->powe);

		ack_error = udpsem_check_error(txpkt, gps_time);
		channel = lrmac_get_channel_by_freq((long)(txpkt->freq * 1E6));
		if(ack_error != UDPSEM_ERROR_TX_FREQ && ack_error != UDPSEM_ERROR_TX_POWER){
			if(txpkt->imme == true){ /** forward immediately */
				LOG_WARN(TAG, "Forward down link immediately");
				lrmac_phys_setting_t phys_setting;
				phys_setting.freq = (long)(txpkt->freq * 1E6);
				phys_setting.powe = txpkt->powe;
				phys_setting.sf   = txpkt->sf;
				phys_setting.bw   = txpkt->bw;
				phys_setting.codr = txpkt->codr;
				phys_setting.prea = txpkt->prea;
				phys_setting.crc  = txpkt->ncrc;
				phys_setting.iiq  = txpkt->ipol;
				lrmac_apply_setting(channel, &phys_setting);

				lrmac_macpkt_t sendpacket;
				sendpacket.channel = channel;
				sendpacket.payload_size = txpkt->size;
				base64_decode(txpkt->data, txpkt->size, sendpacket.payload);
				lrmac_forward_downlink(&sendpacket);

				lrmac_restore_default_setting(channel);
			}
			else{ /** Schedule down link*/
				LOG_WARN(TAG, "Forward down link with schedule");
			}
		}

		udpsem_send_tx_ack(&pgtw->udpsemtech, ack_error);
		free(downlink_pkt);
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
			LOG_EVENT(TAG, "Received downlink message");
		break;
		case UDPSEM_EVENTID_KEEPALIVE:
			LOG_EVENT(TAG, "Keep alive connection, free heap = %lubytes", dev_get_free_heap_size());
		break;
		default:
		break;
	};
}






