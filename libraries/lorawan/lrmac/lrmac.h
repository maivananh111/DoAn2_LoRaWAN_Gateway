/*
 * lrmac.h
 *
 *  Created on: Nov 20, 2023
 *      Author: anh
 */

#ifndef LORAWAN_LRMAC_LRMAC_H_
#define LORAWAN_LRMAC_LRMAC_H_

#ifdef __cplusplus
extern "C"{
#endif

#include "lorawan/lrphys/lrphys.h"



typedef struct{
	uint8_t          channel      = 0;
	lrphys_eventid_t eventid      = LRPHYS_ERROR_CRC;
	uint8_t          *payload     = NULL;
	uint8_t          payload_size = 0;
} lrmac_macpkt_t;


typedef struct{
	lrphys *phys;
	long freq;
	long bw;
	uint8_t sf;
	uint8_t cdr;
	int8_t rssi;
	int8_t snr;
} lrmac_phys_info_t;



void lrmac_initialize(void);

void lrmac_get_phys_info(uint8_t channel, lrmac_phys_info_t *info);

bool lrmac_link_physical(lrphys *phys, lrphys_hwconfig_t *hwconf, uint8_t channel = 0);

void lrmac_forward_downlink(lrmac_macpkt_t *pkt);

uint8_t lrmac_rxpacket_available(lrmac_macpkt_t *ppkt);

#ifdef __cplusplus
}
#endif

#endif /* LORAWAN_LRMAC_LRMAC_H_ */
