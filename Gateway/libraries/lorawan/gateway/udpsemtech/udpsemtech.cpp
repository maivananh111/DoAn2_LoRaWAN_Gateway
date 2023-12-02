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
#include <lwip/netdb.h>
#include "lwip/apps/sntp_opts.h"
#include "lwip/apps/sntp.h"




using json = nlohmann::json;

static const char *TAG = "LoRaWAN";

static void  udpsem_random_token(udpsem_t *pudp);

static err_t udpsem_send(udpsem_t *pudp, uint8_t *buf, uint16_t len);
static void  udpsem_received_handler(void *arg, struct udp_pcb *pcb, struct pbuf *pbuf, const ip_addr_t *addr, u16_t port);

static void  udpsem_config_header(udpsem_t *pudp, udpsem_header_id_t headerid);
static void  udpsem_set_timestamp(udpsem_t *pudp);

static int   udpsem_add_stat_feild(udpsem_t *pudp, uint16_t index);
static int   udpsem_add_rxpk_feild(udpsem_t *pudp, uint16_t index, udpsem_rxpk_t *pkt);
static int   udpsem_add_txpk_ack_feild(udpsem_t *pudp, uint16_t index, const char *error);
static const char *udpsem_enum_to_error_str(udpsem_txpk_ack_error_t error);

static bool  udpsem_parse_pull_resp(uint8_t *buffer, udpsem_txpk_t *txpkt);



void  udpsem_initialize(udpsem_t *pudp, udpsem_server_info_t *server_info, udpsem_gateway_info_t *gtw_info){
	pudp->server_info = server_info;
	pudp->gtw_info = gtw_info;

	pudp->queue_pull_resp  = xQueueCreate(LRWGW_PHYS_TXPKT_QUEUE_SIZE, sizeof(uint8_t *));
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

    LOG_INFO(TAG, "Start LoRaWAN gateway loop.");

    return ERR_OK;
}

err_t udpsem_disconnect(udpsem_t *pudp){
	udp_disconnect(pudp->udp);

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

//    LOG_WARN(TAG, "%s", (char *)(pudp->req_buffer + LRWGW_HEADER_LENGTH));

	err_t ret = udpsem_send(pudp, pudp->req_buffer, index+1);
	if(ret == ERR_OK){
		if(pudp->event_handler != NULL){
			udpsem_event_t event = {
				.eventid = UDPSEM_EVENTID_SENT_DATA,
				.version = (udpsem_protocol_version_t)pudp->server_info->udpver,
				.token   = (uint16_t)((pudp->req_buffer[1]<<8) | pudp->req_buffer[2]),
				.data    = (void *)(pudp->req_buffer),
			};
			pudp->event_handler(pudp, event, pudp->event_parameter);
		}
		pudp->txnb++;
	}
	pudp->ackr = ((float)pudp->ackn / (float)pudp->txnb) / 100.0;

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
				.data    = (void *)(pudp->req_buffer),
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
				.data    = NULL,
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
				.data    = (void *)(pudp->req_buffer),
			};
			pudp->event_handler(pudp, event, pudp->event_parameter);
		}
	}


	return ERR_OK;
}

uint8_t udpsem_txpkt_available(udpsem_t *pudp, udpsem_txpk_t *ptxpkt){
	return (xPortIsInsideInterrupt())?
			(uint8_t)xQueueReceiveFromISR(pudp->queue_pull_resp, &ptxpkt, NULL) :
			(uint8_t)xQueueReceive(pudp->queue_pull_resp, &ptxpkt, 10);
}

bool udpsem_is_modu_lora(udpsem_txpk_t *ptxpkt){
	if(strcmp(ptxpkt->modu, "LORA") == 0) return true;
	return false;
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

    if(pbuf != NULL) {
    	uint8_t *u8buf = (uint8_t *)pbuf->payload;
    	udpsem_txpk_t *txpkt = NULL;
    	udpsem_header_id_t header = (udpsem_header_id_t)u8buf[3];

    	udpsem_event_t event = {
        	.version = (udpsem_protocol_version_t)u8buf[0],
			.token   = (uint16_t)((u8buf[1]<<8) | u8buf[2]),
			.data    = NULL,
        };


    	switch(header){
    		case UDPSEM_HEADERID_PUSH_ACK:
    			event.eventid = UDPSEM_EVENTID_RECV_ACK;
    			pudp->ackn++;
    			event.data = NULL;
			break;

    		case UDPSEM_HEADERID_PULL_ACK:
    			event.eventid = UDPSEM_EVENTID_RECV_ACK;
    			event.data = NULL;
			break;

    		case UDPSEM_HEADERID_PULL_PESP:
    			txpkt = (udpsem_txpk_t *)malloc(sizeof(udpsem_txpk_t));
    			if(txpkt == NULL){
    				LOG_ERROR(TAG, "Memory exhausted, malloc fail at %s -> %d", __FUNCTION__, __LINE__);
    				return;
    			}
    			if(udpsem_parse_pull_resp(u8buf, txpkt) == false) return;

    			if((xPortIsInsideInterrupt())?
    					xQueueSendFromISR(pudp->queue_pull_resp, &txpkt, NULL):
						xQueueSend(pudp->queue_pull_resp, &txpkt, 10) == pdFALSE){
    				LOG_ERROR(TAG, "Queue exhausted, send to queue fail at %s -> %d", __FUNCTION__, __LINE__);
    			}

    			event.eventid = UDPSEM_EVENTID_RECV_DATA;
    			event.data    = (void *)u8buf,
    			pudp->dwnb++;
    			pudp->txack_token = event.token;
			break;

    		default:

    		break;
    	};

        pbuf_free(pbuf);
    	pudp->event_handler(pudp, event, pudp->event_parameter);
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
    struct timeval ntpTime;
    uint32_t sec, us;

    /** Get time from NTP server */
    SNTP_GET_SYSTEM_TIME(sec, us);
    ntpTime.tv_sec = sec;
    ntpTime.tv_usec = us;

    /** Convert to UTC time */
    struct tm *utc = gmtime(&ntpTime.tv_sec);
    time_t utc_time = mktime(utc) + LRWGW_TIME_UTC_OFFSET_SEC;
    strftime(pudp->utc_time, sizeof pudp->utc_time, "%F %T %Z", gmtime(&utc_time));
    /** Convert to GPS time */
    struct tm gps_epoch = {
		.tm_mday = 6,
		.tm_mon = 0,
		.tm_year = 1980 - 1900,
    };
    pudp->gps_time = (uint64_t)(mktime(utc) - mktime(&gps_epoch) + 18);
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
	uint8_t *base64_out = (uint8_t *)malloc(LRWGW_BUFFER_SIZE);

	base64_encode(pkt->data, pkt->size, base64_out);

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
				"\"tmst\":%llu"\
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
		pudp->gps_time
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



static bool udpsem_parse_pull_resp(uint8_t *buffer, udpsem_txpk_t *txpkt){
	std::string datr = "";
	std::string modu = "";
	std::string data = "";
	uint8_t modu_len = 0;
	uint8_t data_len = 0;


	json jsonData = json::parse(buffer);
    if (jsonData.is_discarded()) {
        LOG_ERROR(TAG, "Error parse JSON data");
        return false;
    }


	txpkt->imme = (jsonData.find("imme") != jsonData.end())? (bool)       jsonData["imme"]:false;
	txpkt->tmst = (jsonData.find("tmst") != jsonData.end())? (uint32_t)   jsonData["tmst"]:0;
	txpkt->tmms = (jsonData.find("tmms") != jsonData.end())? (uint32_t)   jsonData["tmms"]:0;
	txpkt->rfch = (jsonData.find("rfch") != jsonData.end())? (uint16_t)   jsonData["rfch"]:0;
	txpkt->freq = (jsonData.find("freq") != jsonData.end())? (float)      jsonData["freq"]:0.0;
	txpkt->powe = (jsonData.find("powe") != jsonData.end())? (uint8_t)    jsonData["powe"]:14;
	modu        = (jsonData.find("modu") != jsonData.end())? (std::string)jsonData["modu"]:"LORA";
	datr        = (jsonData.find("datr") != jsonData.end())? (std::string)jsonData["datr"]:"SF7BW125";
	txpkt->codr = (jsonData.find("codr") != jsonData.end())? (uint8_t)    jsonData["codr"]:5;
	txpkt->prea = (jsonData.find("prea") != jsonData.end())? (uint32_t)   jsonData["prea"]:8;
	txpkt->fdev = (jsonData.find("fdev") != jsonData.end())? (uint32_t)   jsonData["fdev"]:0;
	txpkt->ipol = (jsonData.find("ipol") != jsonData.end())? (bool)       jsonData["ipol"]:false;
	txpkt->ncrc = (jsonData.find("ncrc") != jsonData.end())? (bool)       jsonData["ncrc"]:false;
	data        =                                            (std::string)jsonData["data"];
	txpkt->size =                                            (uint8_t)    jsonData["size"];

	modu_len = strlen((char *)modu.c_str());
	txpkt->modu = (char *)malloc(modu_len * sizeof(char) + 1);
	memcpy(txpkt->modu, (char *)modu.c_str(), modu_len);
	txpkt->modu[modu_len] = 0;

	data_len = strlen((char *)data.c_str());
	txpkt->data = (uint8_t *)malloc(data_len * sizeof(uint8_t) + 1);
	memcpy(txpkt->data, (char *)data.c_str(), data_len);
	txpkt->data[data_len] = 0;

	sscanf(datr.c_str(), "SF%hhuBW%hu", &txpkt->sf, &txpkt->bw);

	return true;
}


