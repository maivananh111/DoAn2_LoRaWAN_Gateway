/*
 * gateway.h
 *
 *  Created on: Nov 13, 2023
 *      Author: anh
 */

#ifndef LORAWAN_GATEWAY_GATEWAY_H_
#define LORAWAN_GATEWAY_GATEWAY_H_


#ifdef __cplusplus
extern "C"{
#endif

#include "stdlib.h"

#include "lwip/err.h"
#include "lwip/udp.h"

#include "lorawan/gateway/gateway_config.h"
#include "lorawan/gateway/udpsemtech/udpsemtech.h"



typedef enum{
	LORAWAN_GATEWAY_CONNECT,
	LORAWAN_GATEWAY_DISCONNECT,
	LORAWAN_GATEWAY_EVENT_DOWNLINK,
	LORAWAN_GATEWAY_EVENT_UPLINK,
} lorawan_gateway_event_t;

typedef struct lorawan_gateway lorawan_gateway_t;
struct lorawan_gateway{
	udpsem_server_info_t server_info;
	udpsem_gateway_info_t gateway_info;
	udpsem_t udpsemtech;

	uint8_t stat_interval = LRWGW_STAT_INTERVAL;
	uint8_t keepalive_interval = LRWGW_KEEP_ALIVE;

	void (*event_handler)(lorawan_gateway_t *pgtw, lorawan_gateway_event_t event, void *param);
	void *event_parameter = NULL;
};



void lorawan_gateway_initialize(lorawan_gateway_t *pgtw);
void lorawan_gateway_register_event_handler(lorawan_gateway_t *pgtw,
		void (*event_handler_function)(lorawan_gateway_t *pgtw, lorawan_gateway_event_t event, void *param),
		void *param);

void lorawan_gateway_set_identify(lorawan_gateway_t *pgtw, uint64_t id);
void lorawan_gateway_set_coordinate(lorawan_gateway_t *pgtw, float latitude, float longitude, int altitude);

err_t lorawan_gateway_start(lorawan_gateway_t *pgtw);
void lorawan_gateway_stop(lorawan_gateway_t *pgtw);



#ifdef __cplusplus
}
#endif


#endif /* LORAWAN_GATEWAY_H_ */
