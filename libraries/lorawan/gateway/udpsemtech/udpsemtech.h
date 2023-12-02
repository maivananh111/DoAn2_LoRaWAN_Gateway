/*
 * udpsemtech.h
 *
 *  Created on: Nov 25, 2023
 *      Author: anh
 */

#ifndef LORAWAN_GATEWAY_UDPSEMTECH_UDPSEMTECH_H_
#define LORAWAN_GATEWAY_UDPSEMTECH_UDPSEMTECH_H_

#ifdef __cplusplus
extern "C"{
#endif

#include "stdlib.h"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include "lwip/err.h"
#include "lwip/udp.h"

#include "lorawan/gateway/gateway_config.h"




/**
 * UdpSemtech protocol version.
 */
typedef enum{
	UDPSEM_PROTOVER_1 = 0x01,
	UDPSEM_PROTOVER_2 = 0x02,
} udpsem_protocol_version_t;

/**
 * UdpSemtech header id.
 */
typedef enum{
	/** Upstream identify */
	UDPSEM_HEADERID_PUSH_DATA = 0x00,
	UDPSEM_HEADERID_PUSH_ACK  = 0x01,
	/** Downstream identify */
	UDPSEM_HEADERID_PULL_DATA = 0x02,
	UDPSEM_HEADERID_PULL_PESP = 0x03,
	UDPSEM_HEADERID_PULL_ACK  = 0x04,

	UDPSEM_HEADERID_TX_ACK    = 0x05,
} udpsem_header_id_t;

/**
 * UdpSemtech event id.
 */
typedef enum{
	/** Down stream event */
	UDPSEM_EVENTID_RECV_DATA,
	UDPSEM_EVENTID_RECV_ACK,
	UDPSEM_EVENTID_KEEPALIVE,
	/** Up stream event */
	UDPSEM_EVENTID_SENT_STATE,
	UDPSEM_EVENTID_SENT_DATA,
	UDPSEM_EVENTID_SENT_ACK,
} udpsem_event_id_t;

typedef enum{
	UDPSEM_ERROR_NONE,
	UDPSEM_ERROR_TOO_LATE,
	UDPSEM_ERROR_TOO_EARLY,
	UDPSEM_ERROR_TX_POWER,
	UDPSEM_ERROR_TX_FREQ,
} udpsem_txpk_ack_error_t;

/**
 * Event handle data struct.
 */
typedef struct{
	udpsem_event_id_t         eventid = UDPSEM_EVENTID_KEEPALIVE;
	udpsem_protocol_version_t version = UDPSEM_PROTOVER_2;
	uint16_t 		          token   = 0;
	void     		          *data   = NULL;
} udpsem_event_t;


/**
 * UdpSemtech config.
 */
typedef struct{
	/** TTN Server name */
	const char 			 			*ttn_server = LRWGW_DEFAULT_TTN_SERVER;
	/** Port to connect to server */
	const int               		port        = LRWGW_DEFAULT_TTN_PORT;
	/** UDP forward protocol version */
	const udpsem_protocol_version_t udpver      = LRWGW_DEFAULT_PROTO_VER;
	/** NTP server */
	const char           			*ntp_server = LRWGW_DEFAULT_NTP_SERVER;

} udpsem_server_info_t;

typedef struct{
	/** Gateway identify */
	uint64_t   id           = LRWGW_DEFAULT_ID;
	/** Coordinate */
    float      latitude;
    float      longitude;
    int        altitude;
	/** Information*/
	const char *platform    = LRWGW_DEFAULT_PLATFORM;
	const char *mail;
	const char *description = LRWGW_DEFAULT_DESCRIPTION;
} udpsem_gateway_info_t;


typedef struct udpsem_handler udpsem_t;
typedef void(*udpsem_event_handler_f)(udpsem_t *pudp, udpsem_event_t event, void *param);





struct udpsem_handler{
	udpsem_server_info_t *server_info;
	udpsem_gateway_info_t *gtw_info;

	udpsem_event_handler_f event_handler = NULL;
	void *event_parameter = NULL;

	QueueHandle_t queue_pull_resp;

	struct udp_pcb *udp;
	ip_addr_t ttn_server_ip;
	ip_addr_t ntp_server_ip;

    char      latitude[10];
    char      longitude[10];
    char      altitude[10];

    uint16_t txack_token;

	uint8_t  req_buffer[LRWGW_BUFFER_SIZE];
	char     utc_time[40];
	uint64_t gps_time;

	uint32_t rxnb; //| number | Number of radio packets received (unsigned integer)
	uint32_t rxok; //| number | Number of radio packets received with a valid PHY CRC
	uint32_t rxfw; //| number | Number of radio packets forwarded (unsigned integer)
	float    ackr; //| number | Percentage of upstream datagrams that were acknowledged
	uint32_t dwnb; //| number | Number of downlink datagrams received (unsigned integer)
	uint32_t txnb; //| number | Number of packets emitted (unsigned integer)

	uint32_t ackn; //| number | Number of push acknowledge.
};


typedef struct{
	uint8_t  channel 	  = 0;
	uint16_t rf_chain 	  = 0;
	float    freq 		  = 923.00000;
	int8_t   crc_stat 	  = 1;
	uint8_t  sf 		  = 7;
	uint16_t bw 		  = 125;
	uint8_t  codr 		  = 5;
	int8_t   rssi 		  = -1;
	int8_t   snr          = -1;
	uint8_t  *data 	      = NULL;
	uint8_t  size         = 23;
} udpsem_rxpk_t;


typedef struct{
	bool     imme 	      = false;//
	uint32_t tmst         = 0;//
	uint32_t tmms         = 0;//
	uint16_t rfch 	      = 0;//
	float    freq 		  = 923.00000;//
	uint8_t  powe         = 20;//
	char     *modu        = (char *)"LORA";//
	uint8_t  sf 		  = 7;//
	uint16_t bw 		  = 125;//
	uint8_t  codr 		  = 5;//
	uint32_t prea         = 8;//
	uint32_t fdev         = 0;//
	bool     ipol         = false;//
	bool     ncrc  		  = false;
	uint8_t  *data        = NULL;
	uint8_t  size  		  = 23;
} udpsem_txpk_t;


void  udpsem_initialize(udpsem_t *pudp, udpsem_server_info_t *server_info, udpsem_gateway_info_t *gtw_info);

err_t udpsem_connect(udpsem_t *pudp);
err_t udpsem_disconnect(udpsem_t *pudp);

void udpsem_register_event_handler(udpsem_t *pudp, udpsem_event_handler_f event_handler_function, void *param);

uint8_t udpsem_txpkt_available(udpsem_t *pudp, udpsem_txpk_t *ptxpkt);

err_t udpsem_push_data(udpsem_t *pudp, udpsem_rxpk_t *prxpkt, uint8_t incl_stat);
err_t udpsem_send_stat(udpsem_t *pudp);
err_t udpsem_keepalive(udpsem_t *pudp);
err_t udpsem_send_tx_ack(udpsem_t *pudp, udpsem_txpk_ack_error_t error);

bool udpsem_is_modu_lora(udpsem_txpk_t *ptxpkt);


#ifdef __cplusplus
}
#endif

#endif /* LORAWAN_GATEWAY_UDPSEMTECH_UDPSEMTECH_H_ */
