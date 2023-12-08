/*
 * gateway_internal.cpp
 *
 *  Created on: Nov 25, 2023
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
#include "lwip/netdb.h"
#include "lwip/apps/sntp_opts.h"
#include "lwip/apps/sntp.h"

#include "rtc.h"


using json = nlohmann::json;
using string = std::string;

static const char *TAG = "LoRaWAN";
static RTC_TimeTypeDef rtc_time;
static RTC_DateTypeDef rtc_date;

static void  udpsem_random_token(udpsem_t *pudp);

static err_t udpsem_send(udpsem_t *pudp, uint8_t *buf, uint16_t len);
static void  udpsem_received_handler(void *arg, struct udp_pcb *pcb, struct pbuf *pbuf, const ip_addr_t *addr, u16_t port);

static void  udpsem_config_header(udpsem_t *pudp, udpsem_header_id_t headerid);
static void  udpsem_set_timestamp(udpsem_t *pudp);

static int   udpsem_add_stat_feild(udpsem_t *pudp, uint16_t index);
static int   udpsem_add_rxpk_feild(udpsem_t *pudp, uint16_t index, udpsem_rxpk_t *pkt);
static int   udpsem_add_txpk_ack_feild(udpsem_t *pudp, uint16_t index, const char *error);
static const char *udpsem_enum_to_error_str(udpsem_txpk_ack_error_t error);

static void  udpsem_update_rtc(void);


void  udpsem_initialize(udpsem_t *pudp, udpsem_server_info_t *server_info, udpsem_gateway_info_t *gtw_info, QueueHandle_t *pqueue){
	pudp->server_info = server_info;
	pudp->gtw_info    = gtw_info;
	pudp->pqueue_resp = pqueue;
}

err_t udpsem_connect(udpsem_t *pudp){
	struct hostent *host;

	/** Resolve ttn host name to IP address */
	host = lwip_gethostbyname(pudp->server_info->ttn_server);
	if (host == NULL) {
		LOG_ERROR(TAG, "Error resolve ttn host name to address info.");
		return ERR_CONN;
	}
	else {
	    struct in_addr **addr_list = (struct in_addr **)host->h_addr_list;
	    if (addr_list[0] != NULL) {
	    	in_addr_t addr = inet_addr(inet_ntoa(*addr_list[0]));
	    	IP_ADDR4(&pudp->ttn_server_ip, (uint8_t)(addr & 0xff), (uint8_t)((addr >> 8) & 0xff), (uint8_t)((addr >> 16) & 0xff), (uint8_t)((addr >> 24) & 0xff));
	    	LOG_INFO(TAG, "Resolved %s to ip address: %s", pudp->server_info->ttn_server, ip4addr_ntoa((const ip4_addr_t *)&pudp->ttn_server_ip));
	    }
	    else{
			LOG_ERROR(TAG, "Error ip address invalid.");
			return ERR_CONN;
	    }
	}
	/** Resolve ntp host name to IP address */
	host = lwip_gethostbyname(pudp->server_info->ntp_server);
	if (host == NULL) {
		LOG_ERROR(TAG, "Error resolve ntp host name to address info.");
		return ERR_CONN;
	}
	else {
	    struct in_addr **addr_list = (struct in_addr **)host->h_addr_list;
	    if (addr_list[0] != NULL) {
	    	in_addr_t addr = inet_addr(inet_ntoa(*addr_list[0]));
	    	IP_ADDR4(&pudp->ntp_server_ip, (uint8_t)(addr & 0xff), (uint8_t)((addr >> 8) & 0xff), (uint8_t)((addr >> 16) & 0xff), (uint8_t)((addr >> 24) & 0xff));
	    	LOG_INFO(TAG, "Resolved %s to ip address: %s", pudp->server_info->ntp_server, ip4addr_ntoa((const ip4_addr_t *)&pudp->ntp_server_ip));
	    }
	    else{
			LOG_ERROR(TAG, "Error ip address invalid.");
			return ERR_CONN;
	    }
	}

	/** New ttn UDP connection */
	pudp->udp = udp_new();
    udp_recv(pudp->udp, udpsem_received_handler, pudp);
    udp_bind(pudp->udp, IP_ADDR_ANY, 0);
    if(udp_connect(pudp->udp, &pudp->ttn_server_ip, pudp->server_info->port) != ERR_OK){
    	LOG_ERROR(TAG, "Error connect to server %s, port %d.", pudp->server_info->ttn_server, pudp->server_info->port);
    	return ERR_CONN;
    }
    LOG_INFO(TAG, "Connected to ttn server %s, port %d", pudp->server_info->ttn_server, pudp->server_info->port);

    /** Connect to NTP server */
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setserver(0, &pudp->ntp_server_ip);
    sntp_init();
    LOG_INFO(TAG, "Connected to ntp server %s, port %d", pudp->server_info->ntp_server, SNTP_PORT);

    LOG_INFO(TAG, "Waiting for setup......");

    vTaskDelay(5000);
    udpsem_update_rtc();

	HAL_RTC_GetTime(&hrtc, &rtc_time, RTC_FORMAT_BIN);
	HAL_RTC_GetDate(&hrtc, &rtc_date, RTC_FORMAT_BIN);
	LOG_INFO("RTC", "%02d/%02d/20%02d - %02d:%02d:%02d %d",
		  rtc_date.Date, rtc_date.Month, rtc_date.Year,
		  rtc_time.Hours, rtc_time.Minutes, rtc_time.Seconds, rtc_date.WeekDay);

    LOG_INFO(TAG, "Start LoRaWAN gateway loop.");

    return ERR_OK;
}

err_t udpsem_disconnect(udpsem_t *pudp){
	udp_disconnect(pudp->udp);
	udp_remove(pudp->udp);
	sntp_stop();

	return ERR_OK;
}

void udpsem_register_event_handler(udpsem_t *pudp, udpsem_event_handler_f event_handler_function, void *param){
	pudp->event_handler = event_handler_function;
	pudp->event_parameter = param;
}


/**
 * UpStream.
 */
err_t udpsem_push_data(udpsem_t *pudp, udpsem_rxpk_t *prxpkt, uint8_t incl_stat){
	int index = 0;

	udpsem_config_header(pudp, UDPSEM_HEADERID_PUSH_DATA);
	udpsem_set_timestamp(pudp);

    pudp->req_buffer[LRWGW_HEADER_LENGTH] = '{';
    index = LRWGW_HEADER_LENGTH+1 + udpsem_add_rxpk_feild(pudp, LRWGW_HEADER_LENGTH + 1, prxpkt);

    if(incl_stat){
		pudp->req_buffer[index] = ',';
		index = index+1 + udpsem_add_stat_feild(pudp, index + 1);
    }

    pudp->req_buffer[index] = '}';
    pudp->req_buffer[index+1] = 0;

//    LOG_WARN(TAG, "%s", (char *)(pudp->req_buffer + 12));


	err_t ret = udpsem_send(pudp, pudp->req_buffer, index+1);
	if(ret == ERR_OK){
		if(pudp->event_handler != NULL){
			udpsem_event_t event = {
				.eventid = UDPSEM_EVENTID_SENT_DATA,
				.version = (udpsem_protocol_version_t)pudp->server_info->udpver,
				.token   = (uint16_t)((pudp->req_buffer[1]<<8) | pudp->req_buffer[2]),
				.data    = (void *)pudp->req_buffer,
			};
			pudp->event_handler(pudp, event, pudp->event_parameter);
		}
		pudp->txnb++;
	}
	pudp->ackr = ((double)pudp->ackn / (double)pudp->txnb) / 100.0;

	return ERR_OK;
}

err_t udpsem_send_stat(udpsem_t *pudp){
	int index = 0;

	udpsem_config_header(pudp, UDPSEM_HEADERID_PUSH_DATA);
    udpsem_set_timestamp(pudp);

    pudp->req_buffer[LRWGW_HEADER_LENGTH] = '{';
    index = LRWGW_HEADER_LENGTH + udpsem_add_stat_feild(pudp, LRWGW_HEADER_LENGTH + 1) + 1;
    pudp->req_buffer[index] = '}';
    pudp->req_buffer[index+1] = 0;

	err_t ret = udpsem_send(pudp, pudp->req_buffer, index+1);
	if(ret == ERR_OK){
		if(pudp->event_handler != NULL){
			udpsem_event_t event = {
				.eventid = UDPSEM_EVENTID_SENT_STATE,
				.version = (udpsem_protocol_version_t)pudp->server_info->udpver,
				.token   = (uint16_t)((pudp->req_buffer[1]<<8) | pudp->req_buffer[2]),
				.data    = (void *)pudp->req_buffer,
			};
			pudp->event_handler(pudp, event, pudp->event_parameter);
		}
	}

	return ret;
}

/**
 * DownStream.
 */
err_t udpsem_keepalive(udpsem_t *pudp){
	udpsem_config_header(pudp, UDPSEM_HEADERID_PULL_DATA);
	pudp->req_buffer[LRWGW_HEADER_LENGTH] = 0;

	err_t ret = udpsem_send(pudp, pudp->req_buffer, LRWGW_HEADER_LENGTH);
	if(ret == ERR_OK){
		if(pudp->event_handler != NULL){
			udpsem_event_t event = {
				.eventid = UDPSEM_EVENTID_KEEPALIVE,
				.version = (udpsem_protocol_version_t)pudp->server_info->udpver,
				.token   = (uint16_t)((pudp->req_buffer[1]<<8) | pudp->req_buffer[2]),
				.data    = (void *)pudp->req_buffer,
			};
			pudp->event_handler(pudp, event, pudp->event_parameter);
		}
	}

	return ERR_OK;
}

err_t udpsem_send_tx_ack(udpsem_t *pudp, udpsem_txpk_ack_error_t error){
	int index = 0;

	udpsem_config_header(pudp, UDPSEM_HEADERID_TX_ACK);
    pudp->req_buffer[1]  = (uint8_t)((pudp->txack_token>>8) & 0xFF);
    pudp->req_buffer[2]  = (uint8_t)(pudp->txack_token & 0xFF);

    const char *error_str = udpsem_enum_to_error_str(error);
	udpsem_add_txpk_ack_feild(pudp, LRWGW_HEADER_LENGTH, error_str);

	pudp->req_buffer[index] = 0;

	err_t ret = udpsem_send(pudp, pudp->req_buffer, index);
	if(ret == ERR_OK){
		if(pudp->event_handler != NULL){
			udpsem_event_t event = {
				.eventid = UDPSEM_EVENTID_SENT_ACK,
				.version = (udpsem_protocol_version_t)pudp->server_info->udpver,
				.token   = (uint16_t)((pudp->req_buffer[1]<<8) | pudp->req_buffer[2]),
				.data    = (void *)pudp->req_buffer,
			};
			pudp->event_handler(pudp, event, pudp->event_parameter);
		}
	}
//	LOG_WARN(TAG, "Send: %s", pudp->req_buffer + LRWGW_HEADER_LENGTH);

	return ERR_OK;
}

BaseType_t udpsem_txpkt_available(udpsem_t *pudp, udpsem_txpk_t *ptxpkt){
	return (xPortIsInsideInterrupt())?
		    xQueueReceiveFromISR(*pudp->pqueue_resp, &ptxpkt, NULL) :
			xQueueReceive(*pudp->pqueue_resp, &ptxpkt, 10);
}

udpsem_txpk_ack_error_t  udpsem_check_error(udpsem_txpk_t *ptxpkt, uint32_t current_time){
	/** Check frequency */
	if(ptxpkt->freq < LRWGW_FREQ_MIN || ptxpkt->freq > LRWGW_FREQ_MAX){
		LOG_DEBUG(TAG, "Down link invalid frequency");
		return UDPSEM_ERROR_TX_FREQ;
	}
	/** Check power */
	if(ptxpkt->powe < LRWGW_POWER_MIN || ptxpkt->powe > LRWGW_POWER_MAX){
		LOG_DEBUG(TAG, "Down link invalid tx power");
		return UDPSEM_ERROR_TX_POWER;
	}
	/** Check response time */
//	if(ptxpkt->tmst < current_time){
//		LOG_DEBUG(TAG, "Down link message too early");
//		return UDPSEM_ERROR_TOO_EARLY;
//	}
//	else{
//		LOG_DEBUG(TAG, "Down link message to late");
//		return UDPSEM_ERROR_TOO_LATE;
//	}

	return UDPSEM_ERROR_NONE;
}

uint32_t udpsem_get_ntp_gps_time(void){
	uint32_t gps_time;
	struct tm utc;

    HAL_RTC_GetTime(&hrtc, &rtc_time, RTC_FORMAT_BIN);
    HAL_RTC_GetDate(&hrtc, &rtc_date, RTC_FORMAT_BIN);

    utc.tm_hour = rtc_time.Hours;
    utc.tm_min  = rtc_time.Minutes;
    utc.tm_sec  = rtc_time.Seconds;
    utc.tm_mday = rtc_date.Date;
    utc.tm_mon  = rtc_date.Month;
    utc.tm_year = rtc_date.Year + 100;
    utc.tm_wday = rtc_date.WeekDay;

    struct tm gps_epoch = {
		.tm_mday = 6,
		.tm_mon = 0,
		.tm_year = 1980 - 1900,
    };

    gps_time = (uint32_t)(mktime(&utc) - mktime(&gps_epoch) + 18);

	return gps_time;
}



static void  udpsem_update_rtc(void){
    struct timeval ntpTime;
    uint32_t sec, us;
    /** Get time from NTP server */
    SNTP_GET_SYSTEM_TIME(sec, us);
    ntpTime.tv_sec = sec;
    ntpTime.tv_usec = us;
    /** Convert to UTC time */
    struct tm *utc = gmtime(&ntpTime.tv_sec);
    time_t utc_time = mktime(utc) + LRWGW_TIME_UTC_OFFSET_SEC;
    struct tm *timeinfo = localtime(&utc_time);

    rtc_time.Hours   = timeinfo->tm_hour;
    rtc_time.Minutes = timeinfo->tm_min;
    rtc_time.Seconds = timeinfo->tm_sec;
    rtc_date.Date    = timeinfo->tm_mday;
    rtc_date.Month   = timeinfo->tm_mon;
    rtc_date.Year    = (uint8_t)(timeinfo->tm_year - 100);
    rtc_date.WeekDay = timeinfo->tm_wday;

    HAL_RTC_SetTime(&hrtc, &rtc_time, RTC_FORMAT_BIN);
    HAL_RTC_SetDate(&hrtc, &rtc_date, RTC_FORMAT_BIN);
}

static void udpsem_random_token(udpsem_t *pudp){
	uint32_t val;
	extern RNG_HandleTypeDef hrng;

	HAL_RNG_GenerateRandomNumber(&hrng, &val);

	pudp->req_buffer[1]  = (uint8_t)((val>>8) & 0xFF);
	pudp->req_buffer[2]  = (uint8_t)(val & 0xFF);
}


static err_t udpsem_send(udpsem_t *pudp, uint8_t *buf, uint16_t len){
	struct pbuf *txBuf = pbuf_alloc(PBUF_TRANSPORT, len, PBUF_RAM);

	if(txBuf != NULL){
		pbuf_take(txBuf, buf, len);
		udp_send(pudp->udp, txBuf);
		pbuf_free(txBuf);
		return ERR_OK;
	}

	return ERR_MEM;
}

static void udpsem_received_handler(void *arg, struct udp_pcb *pcb, struct pbuf *pbuf, const ip_addr_t *addr, u16_t port){
	udpsem_t *pudp = (udpsem_t *)arg;
	udpsem_event_t event;

    if(pbuf != NULL) {
    	uint8_t *udp_buffer = (uint8_t *)pbuf->payload;
    	event.version = (udpsem_protocol_version_t)udp_buffer[0];
		event.token   = (uint16_t)((udp_buffer[1]<<8) | udp_buffer[2]);
		event.data = pbuf->payload;

    	switch((udpsem_header_id_t)udp_buffer[3]){
    		case UDPSEM_HEADERID_PUSH_ACK:
    			event.eventid = UDPSEM_EVENTID_RECV_ACK;
    			pudp->ackn++;
			break;

    		case UDPSEM_HEADERID_PULL_ACK:
    			event.eventid = UDPSEM_EVENTID_RECV_ACK;
			break;

    		case UDPSEM_HEADERID_PULL_PESP:{
    			uint8_t *payload = NULL;

    	    	payload = (uint8_t *)malloc(pbuf->len * sizeof(uint8_t) + 1);
    	    	if(payload == NULL){
    	    		LOG_ERROR(TAG, "Memory exhausted, malloc fail at %s -> %d", __FUNCTION__, __LINE__);
    	    		pbuf_free(pbuf);
    	    		return;
    	    	}
    	    	memset(payload, 0, pbuf->len);
    	    	memcpy(payload, (uint8_t *)pbuf->payload, pbuf->len);
    	    	payload[pbuf->len] = 0;

    			pudp->dwnb++;
    			pudp->txack_token = (uint16_t)((udp_buffer[1]<<8) | udp_buffer[2]);
    			event.eventid = UDPSEM_EVENTID_RECV_DATA;

    			if((xPortIsInsideInterrupt())?
    					xQueueSendFromISR(*pudp->pqueue_resp, &payload, NULL):
						xQueueSend(*pudp->pqueue_resp, &payload, 10) == pdFALSE){
    				LOG_ERROR(TAG, "Queue full, send to queue fail at %s -> %d", __FUNCTION__, __LINE__);
    			}
    		}
			break;

    		default:

    		break;
    	};

    	pudp->event_handler(pudp, event, pudp->event_parameter);
    	pbuf_free(pbuf);
    }
}





static void  udpsem_config_header(udpsem_t *pudp, udpsem_header_id_t headerid){
	if(headerid == UDPSEM_HEADERID_PUSH_ACK || headerid == UDPSEM_HEADERID_PULL_ACK) return;

	pudp->req_buffer[0]  = (uint8_t)((pudp->server_info->udpver));

	udpsem_random_token(pudp);
	pudp->req_buffer[3]  = (uint8_t)headerid;

	pudp->req_buffer[4]  = (uint8_t)((pudp->gtw_info->id>>56) & 0xFF);
	pudp->req_buffer[5]  = (uint8_t)((pudp->gtw_info->id>>48) & 0xFF);
	pudp->req_buffer[6]  = (uint8_t)((pudp->gtw_info->id>>40) & 0xFF);
	pudp->req_buffer[7]  = (uint8_t)((pudp->gtw_info->id>>32) & 0xFF);
	pudp->req_buffer[8]  = (uint8_t)((pudp->gtw_info->id>>24) & 0xFF);
	pudp->req_buffer[9]  = (uint8_t)((pudp->gtw_info->id>>16) & 0xFF);
	pudp->req_buffer[10] = (uint8_t)((pudp->gtw_info->id>>8 ) & 0xFF);
	pudp->req_buffer[11] = (uint8_t)((pudp->gtw_info->id      & 0xFF));
}

static void  udpsem_set_timestamp(udpsem_t *pudp){
/*
    struct timeval ntpTime;
    uint32_t sec, us;

    SNTP_GET_SYSTEM_TIME(sec, us);
    ntpTime.tv_sec = sec;
    ntpTime.tv_usec = us;

    struct tm *utc = gmtime(&ntpTime.tv_sec);
    time_t utc_time = mktime(utc) + LRWGW_TIME_UTC_OFFSET_SEC;
    strftime(pudp->utc_time, sizeof pudp->utc_time, "%F %T %Z", gmtime(&utc_time));

    struct tm gps_epoch = {
		.tm_mday = 6,
		.tm_mon = 0,
		.tm_year = 1980 - 1900,
    };

    pudp->gps_time = (uint32_t)(mktime(utc) - mktime(&gps_epoch) + 18);
*/
	struct tm utc;
	time_t utc_time;

    HAL_RTC_GetTime(&hrtc, &rtc_time, RTC_FORMAT_BIN);
    HAL_RTC_GetDate(&hrtc, &rtc_date, RTC_FORMAT_BIN);

    utc.tm_hour = rtc_time.Hours;
    utc.tm_min  = rtc_time.Minutes;
    utc.tm_sec  = rtc_time.Seconds;
    utc.tm_mday = rtc_date.Date;
    utc.tm_mon  = rtc_date.Month;
    utc.tm_year = rtc_date.Year + 100;
    utc.tm_wday = rtc_date.WeekDay;

    utc_time = mktime(&utc);
    strftime(pudp->utc_time, sizeof pudp->utc_time, "%F %T %Z", gmtime(&utc_time));

    struct tm gps_epoch = {
		.tm_mday = 6,
		.tm_mon = 0,
		.tm_year = 1980 - 1900,
    };

    pudp->gps_time = (uint32_t)(utc_time - mktime(&gps_epoch) + 18);
}

static int udpsem_add_stat_feild(udpsem_t *pudp, uint16_t index){
/**
	{
		"stat":{...}
	}
	 Name |  Type  | Function
	:----:|:------:|--------------------------------------------------------------
	 time | string | UTC 'system' time of the gateway, ISO 8601 'expanded' format
	 lati | number | GPS latitude of the gateway in degree (float, N is +)
	 long | number | GPS latitude of the gateway in degree (float, E is +)
	 alti | number | GPS altitude of the gateway in meter RX (integer)
	 rxnb | number | Number of radio packets received (unsigned integer)
	 rxok | number | Number of radio packets received with a valid PHY CRC
	 rxfw | number | Number of radio packets forwarded (unsigned integer)
	 ackr | number | Percentage of upstream datagrams that were acknowledged
	 dwnb | number | Number of downlink datagrams received (unsigned integer)
	 txnb | number | Number of packets emitted (unsigned integer)
 */
	snprintf(pudp->latitude,  10, "%.05f", pudp->gtw_info->latitude);
	snprintf(pudp->longitude, 10, "%.05f", pudp->gtw_info->longitude);
	snprintf(pudp->altitude,  10, "%d",    pudp->gtw_info->altitude);

	return snprintf((char *)(pudp->req_buffer+index), LRWGW_BUFFER_SIZE-index,
		"\"stat\":{"\
			"\"time\":\"%s\","\
			"\"lati\":%s,"\
			"\"long\":%s,"\
			"\"alti\":%s,"\
			"\"rxnb\":%lu,"\
			"\"rxok\":%lu,"\
			"\"rxfw\":%lu,"\
			"\"ackr\":%.1f,"\
			"\"dwnb\":%lu,"\
			"\"txnb\":%lu,"\
			"\"pfrm\":\"%s\","\
			"\"mail\":\"%s\","\
			"\"desc\":\"%s\""\
		"}",
		pudp->utc_time,
		pudp->latitude,
		pudp->longitude,
		pudp->altitude,
		pudp->rxnb,
		pudp->rxok,
		pudp->rxfw,
		pudp->ackr,
		pudp->dwnb,
		pudp->txnb,
		pudp->gtw_info->platform,
		pudp->gtw_info->mail,
		pudp->gtw_info->description
	);
}

static int udpsem_add_rxpk_feild(udpsem_t *pudp, uint16_t index, udpsem_rxpk_t *pkt){
/**
	{
		"rxpk":[ {...}, ...]
	}
	 Name |  Type  | Function
	:----:|:------:|--------------------------------------------------------------
	 time | string | UTC time of pkt RX, us precision, ISO 8601 'compact' format
	 tmms | number | GPS time of pkt RX, number of milliseconds since 06.Jan.1980
	 tmst | number | Internal timestamp of "RX finished" event (32b unsigned)
	 freq | number | RX central frequency in MHz (unsigned float, Hz precision)
	 chan | number | Concentrator "IF" channel used for RX (unsigned integer)
	 rfch | number | Concentrator "RF chain" used for RX (unsigned integer)
	 stat | number | CRC status: 1 = OK, -1 = fail, 0 = no CRC
	 modu | string | Modulation identifier "LORA" or "FSK"
	 datr | string | LoRa datarate identifier (eg. SF12BW500)
	 datr | number | FSK datarate (unsigned, in bits per second)
	 codr | string | LoRa ECC coding rate identifier
	 rssi | number | RSSI in dBm (signed integer, 1 dB precision)
	 lsnr | number | Lora SNR ratio in dB (signed float, 0.1 dB precision)
	 size | number | RF packet payload size in bytes (unsigned integer)
	 data | string | Base64 encoded RF packet payload, padded
*/
	char *base64_out = (char *)malloc(LRWGW_BUFFER_SIZE);

	bin_to_b64((const uint8_t *)pkt->data, pkt->size, base64_out, 400);

	int i = snprintf((char *)(pudp->req_buffer+index), LRWGW_BUFFER_SIZE-index,
		"\"rxpk\":["\
			"{"\
				"\"chan\":%d,"\
				"\"rfch\":%d,"\
				"\"freq\":%.03f,"\
				"\"stat\":%d,"\
				"\"modu\":\"LORA\","\
				"\"datr\":\"SF%dBW%d\","\
				"\"codr\":\"4/%d\","\
				"\"rssi\":%d,"\
				"\"lsnr\":%d,"\
				"\"size\":%d,"\
				"\"data\":\"%s\","\
				"\"tmst\":%lu"\
			"}"\
		"]",
		pkt->channel,
		pkt->rf_chain,
		pkt->freq,
		pkt->crc_stat,
		pkt->sf, pkt->bw,
		pkt->codr,
		pkt->rssi,
		pkt->snr,
		pkt->size,
		base64_out,
		(uint32_t)pudp->gps_time
	);

	free(base64_out);

	return i;
}

static int udpsem_add_txpk_ack_feild(udpsem_t *pudp, uint16_t index, const char *error){
	json j = {
		{"txpk_ack",
			{
				{"error", error}
			}
		}
	};

	return snprintf((char *)(pudp->req_buffer+index), LRWGW_BUFFER_SIZE-index, "%s", j.dump().c_str());
}

static const char *udpsem_enum_to_error_str(udpsem_txpk_ack_error_t error){
	switch(error){
		case UDPSEM_ERROR_TOO_LATE:
			return "TOO_LATE";
		break;
		case UDPSEM_ERROR_TOO_EARLY:
			return "TOO_EARLY";
		break;
		case UDPSEM_ERROR_TX_POWER:
			return "TX_POWER";
		break;
		case UDPSEM_ERROR_TX_FREQ:
			return "TX_FREQ";
		break;
		default:
			return "NONE";
		break;
	}

	return NULL;
}



bool udpsem_parse_pull_resp(char *buffer, udpsem_txpk_t *txpkt){
	uint8_t modu_len = 0;
	uint8_t data_len = 0;
	json txpk_json;
	json jsonData;


	jsonData = json::parse(buffer);
    if (jsonData.is_discarded()) {
        LOG_ERROR(TAG, "Error parse JSON data");
        return false;
    }

    if(jsonData.find("txpk") != jsonData.end()) txpk_json = jsonData["txpk"];
    else{
    	LOG_ERROR(TAG, "Invalid json data format");
    	return false;
    }

	txpkt->imme = (txpk_json.find("imme") != txpk_json.end())? (bool)       txpk_json["imme"]:false;
	txpkt->tmst = (txpk_json.find("tmst") != txpk_json.end())? (uint32_t)   txpk_json["tmst"]:0;
	txpkt->tmms = (txpk_json.find("tmms") != txpk_json.end())? (uint32_t)   txpk_json["tmms"]:0;
	txpkt->rfch = (txpk_json.find("rfch") != txpk_json.end())? (uint16_t)   txpk_json["rfch"]:0;
	txpkt->freq = (txpk_json.find("freq") != txpk_json.end())? (double)     txpk_json["freq"]:0.0;
	txpkt->powe = (txpk_json.find("powe") != txpk_json.end())? (uint8_t)    txpk_json["powe"]:14;
	string modu = (txpk_json.find("modu") != txpk_json.end())? (string)     txpk_json["modu"]:"LORA";
	string datr = (txpk_json.find("datr") != txpk_json.end())? (string)     txpk_json["datr"]:"SF7BW125";
	string codr = (txpk_json.find("codr") != txpk_json.end())? (string)     txpk_json["codr"]:"";
	txpkt->prea = (txpk_json.find("prea") != txpk_json.end())? (uint32_t)   txpk_json["prea"]:8;
	txpkt->fdev = (txpk_json.find("fdev") != txpk_json.end())? (uint32_t)   txpk_json["fdev"]:0;
	txpkt->ipol = (txpk_json.find("ipol") != txpk_json.end())? (bool)       txpk_json["ipol"]:false;
	txpkt->ncrc = (txpk_json.find("ncrc") != txpk_json.end())? (bool)       txpk_json["ncrc"]:false;
	string data = (txpk_json.find("data") != txpk_json.end())? (string)     txpk_json["data"]:" ";
	txpkt->size = (txpk_json.find("size") != txpk_json.end())? (uint8_t)    txpk_json["size"]:0;

	sscanf(codr.c_str(), "4/%hhu", &txpkt->codr);

	modu_len = strlen((char *)modu.c_str());
	txpkt->modu = (char *)malloc(modu_len * sizeof(char) + 1);
	memcpy(txpkt->modu, (char *)modu.c_str(), modu_len);
	txpkt->modu[modu_len] = 0;

	data_len = strlen((char *)data.c_str());
	txpkt->data = (uint8_t *)malloc(data_len * sizeof(uint8_t) + 1);
	memcpy(txpkt->data, (char *)data.c_str(), data_len);
	txpkt->data[data_len] = 0;

	sscanf(datr.c_str(), "SF%hhuBW%d", &txpkt->sf, &txpkt->bw);

	return true;
}


