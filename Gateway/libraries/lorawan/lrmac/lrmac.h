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
#include "FreeRTOS.h"
#include "queue.h"

typedef struct{
	uint8_t          channel      = 0;
	lrphys_eventid_t eventid      = LRPHYS_ERROR_CRC;
	uint8_t          *payload     = NULL;
	uint8_t          payload_size = 0;
} lrmac_packet_t;

typedef struct{
	long freq;
	uint8_t powe;
	uint8_t sf;
	long bw;
	uint8_t codr;
	uint32_t prea;
	bool crc;
	bool iiq;
} lrmac_phys_setting_t;


typedef struct{
	lrphys *phys;
	long freq;
	long bw;
	uint8_t sf;
	uint8_t cdr;
	int8_t rssi;
	int8_t snr;
} lrmac_phys_info_t;

void lrmac_initialize(QueueHandle_t *pqueue_macpkt);
bool lrmac_link_physical(lrphys *phys, lrphys_hwconfig_t *hwconf, uint8_t channel = 0);
void lrmac_suspend_physical(void);

void lrmac_get_phys_info(uint8_t channel, lrmac_phys_info_t *info);
uint8_t lrmac_get_channel_by_freq(long freq);

void lrmac_apply_setting(uint8_t channel, lrmac_phys_setting_t *phys_settings);
void lrmac_restore_default_setting(uint8_t channel);

void lrmac_send_packet(lrmac_packet_t *pkt);


#ifdef __cplusplus
}
#endif

#endif /* LORAWAN_LRMAC_LRMAC_H_ */
