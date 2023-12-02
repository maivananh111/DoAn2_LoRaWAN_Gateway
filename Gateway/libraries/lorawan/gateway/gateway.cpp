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


/**
 * Function prototype.
 */
static void lrwgw_send_status(lorawan_gateway_t *pgtw);
static void lrwgw_keepalive(lorawan_gateway_t *pgtw);
static void lrwgw_handle_mac_rxpkt(lorawan_gateway_t *pgtw);
static void lrwgw_handle_udp_txpkt(lorawan_gateway_t *pgtw);

static void lrwgw_udpsemtech_event_handler(udpsem_t *phander, udpsem_event_t event, void *param);



/**
 * Function declaration.
 */


void lorawan_gateway_initialize(lorawan_gateway_t *pgtw){
	lrmac_initialize();
	udpsem_initialize(&pgtw->udpsemtech, &pgtw->server_info, &pgtw->gateway_info);
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
	 * Send received packet.
	 */
	lrwgw_handle_mac_rxpkt(pgtw);

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

static void lrwgw_handle_mac_rxpkt(lorawan_gateway_t *pgtw){
	lrmac_macpkt_t *macpkt = NULL;

	if(lrmac_rxpacket_available(macpkt) == pdTRUE){
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
				free(macpkt);

				if(pgtw->event_handler)
					pgtw->event_handler(pgtw, LORAWAN_GATEWAY_EVENT_UPLINK, pgtw->event_parameter);
			}
		}
	}
}

static void lrwgw_handle_udp_txpkt(lorawan_gateway_t *pgtw){
	udpsem_txpk_t *txpkt = NULL;

	if(udpsem_txpkt_available(&pgtw->udpsemtech, txpkt) == pdTRUE){
		if(txpkt == NULL){
			LOG_ERROR(TAG, "NULL pointer at %s -> %d", __FUNCTION__, __LINE__);
			return;
		}


		udpsem_txpk_ack_error_t ack_error = UDPSEM_ERROR_NONE;
		struct timeval ntp_time;
		uint64_t gps_time;
	    uint32_t sec, us;

	    /** Get time from NTP server */
	    SNTP_GET_SYSTEM_TIME(sec, us);
	    ntp_time.tv_sec = sec;
	    ntp_time.tv_usec = us;
	    struct tm *utc = gmtime(&ntp_time.tv_sec);
	    struct tm gps_epoch = {
			.tm_mday = 6,
			.tm_mon = 0,
			.tm_year = 1980 - 1900,
		};
		gps_time = (uint64_t)(mktime(utc) - mktime(&gps_epoch) + 18);

		if(txpkt->tmst == gps_time){ 													  /** Check response time */
			if(udpsem_is_modu_lora(txpkt)){ 											  /** Check module **/
				if(txpkt->freq >= LRWGW_FREQ_MIN && txpkt->freq <= LRWGW_FREQ_MAX){ 	  /** Check frequency */
					if(txpkt->powe >= LRWGW_POWER_MIN && txpkt->powe <= LRWGW_POWER_MAX){ /** Check power */
						if(txpkt->sf >= 7 && txpkt->sf <= 12){
							if(txpkt->codr >= 5 && txpkt->codr <= 8){
								if(txpkt->imme == true){ /** forward immediately */

								}
								else{ /** Schedule down link*/

								}
							}
							else{ /** Bad coding rate */
								ack_error = UDPSEM_ERROR_TOO_LATE;
							}
						}
						else{ /** Bad SF */
							ack_error = UDPSEM_ERROR_TOO_LATE;
						}
					}
					else{ /** Bad Tx Power */
						ack_error = UDPSEM_ERROR_TX_POWER;
					}
				}
				else{ /** Bad Frequency */
					ack_error = UDPSEM_ERROR_TX_FREQ;
				}
			}
			else{ /** Bad Module */
				ack_error = UDPSEM_ERROR_TX_FREQ;
			}
		}
		else if(txpkt->tmst < gps_time){
			ack_error = UDPSEM_ERROR_TOO_EARLY;
		}
		else{
			ack_error = UDPSEM_ERROR_TOO_LATE;
		}

		udpsem_send_tx_ack(&pgtw->udpsemtech, ack_error);
	}
}




static void lrwgw_udpsemtech_event_handler(udpsem_t *pudp, udpsem_event_t event, void *param){
	switch(event.eventid){
		case UDPSEM_EVENTID_SENT_STATE:
			LOG_EVENT(TAG, "Sent gateway status");
		break;
		case UDPSEM_EVENTID_SENT_DATA:
			LOG_EVENT(TAG, "Sent data");
		break;
		case UDPSEM_EVENTID_RECV_DATA:
			LOG_ERROR(TAG, "Received data");
		break;
		case UDPSEM_EVENTID_KEEPALIVE:
			LOG_EVENT(TAG, "Keep alive");
		break;
		default:
		break;
	};
	if(event.data != NULL) {
		LOG_WARN(TAG, "UdpSemtech data: %s, eventid: %d", (char *)((char *)event.data+LRWGW_HEADER_LENGTH), event.eventid);
		if(event.eventid != UDPSEM_EVENTID_SENT_STATE && event.eventid != UDPSEM_EVENTID_SENT_DATA && event.eventid != UDPSEM_EVENTID_SENT_ACK){
			free(event.data);
		}
	}
}






