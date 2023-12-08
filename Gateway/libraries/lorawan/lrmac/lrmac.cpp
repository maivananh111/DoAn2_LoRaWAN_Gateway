/*
 * lrmac.cpp
 *
 *  Created on: Nov 20, 2023
 *      Author: anh
 */

#include "lorawan/gateway/gateway_config.h"
#include "lorawan/lrmac/lrmac.h"

#include "FreeRTOS.h"
#include "queue.h"

#include "stdlib.h"
#include "string.h"

#include "log/log.h"


static const char *TAG = "LoRaMAC";



static lrphys *phys_corresponds_channel[8] = {NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};
static const long phys_channel_freq_table[8] = {
    (long)9232E5L,
	(long)9234E5L,
	(long)9236E5L,
	(long)9238E5L,
	(long)9240E5L,
	(long)9242E5L,
	(long)9244E5L,
	(long)9246E5L
};
static const uint8_t phys_channel_sf_table[8] = {
	LRWGW_CH0_SF,
	LRWGW_CH1_SF,
	LRWGW_CH2_SF,
	LRWGW_CH3_SF,
	LRWGW_CH4_SF,
	LRWGW_CH5_SF,
	LRWGW_CH6_SF,
	LRWGW_CH7_SF
};
static const long phys_channel_bw_table[8] = {
	(long)LRWGW_CH0_BW,
	(long)LRWGW_CH1_BW,
	(long)LRWGW_CH2_BW,
	(long)LRWGW_CH3_BW,
	(long)LRWGW_CH4_BW,
	(long)LRWGW_CH5_BW,
	(long)LRWGW_CH6_BW,
	(long)LRWGW_CH7_BW
};
static const uint8_t phys_channel_cdr_table[8] = {
	LRWGW_CH0_CDR,
	LRWGW_CH1_CDR,
	LRWGW_CH2_CDR,
	LRWGW_CH3_CDR,
	LRWGW_CH4_CDR,
	LRWGW_CH5_CDR,
	LRWGW_CH6_CDR,
	LRWGW_CH7_CDR
};
static lrmac_phys_setting_t phys_settings_table[8];
static QueueHandle_t *pqueue;

static void lrmac_phys_event_handler(void *arg, lrphys_eventid_t id, uint8_t len);
static uint8_t lrmac_get_phys_channel(lrphys *phys);



void lrmac_get_phys_info(uint8_t channel, lrmac_phys_info_t *info){
	if(channel < 0) channel = 0;
	if(channel > 7) channel = 7;

	info->phys = phys_corresponds_channel[channel];
	info->freq = phys_channel_freq_table[channel];
	info->bw   = phys_channel_bw_table[channel];
	info->sf   = phys_channel_sf_table[channel];
	info->cdr  = phys_channel_cdr_table[channel];
	info->rssi = (double)info->phys->packet_rssi();
	info->snr  = (int8_t)info->phys->packet_snr();
}


void lrmac_initialize(QueueHandle_t *pqueue_macpkt){
	pqueue = pqueue_macpkt;

	for(int i=0; i<8; i++){
		phys_settings_table[i].freq = phys_channel_freq_table[i];
		phys_settings_table[i].powe = 20;
		phys_settings_table[i].sf   = phys_channel_sf_table[i];
		phys_settings_table[i].bw   = phys_channel_bw_table[i];
		phys_settings_table[i].codr = phys_channel_cdr_table[i];
		phys_settings_table[i].prea = 8;
		phys_settings_table[i].crc  = false;
		phys_settings_table[i].iiq  = false;
	}
}

bool lrmac_link_physical(lrphys *phys, lrphys_hwconfig_t *hwconf, uint8_t channel){
	if(channel < 0) channel = 0;
	if(channel > 7) channel = 7;
	phys_corresponds_channel[channel] = phys;

	if(!phys->initialize(hwconf)) {
		LOG_ERROR(TAG, "Fail to initialize LoRa physical channel %d", channel);
		return false;
	}
	phys->register_event_handler(lrmac_phys_event_handler, (void *)phys);

	lrmac_restore_default_setting(channel);

	phys->set_mode_receive_it(0);

#if LRWGW_MAC_DEBUG
	LOG_INFO(TAG, "Add LoRa physical to channel %d[freq: %luHz, sf: %d, bw: %lu, codr: 4/%d]",
			channel, phys_channel_freq_table[channel], phys_channel_sf_table[channel], phys_channel_bw_table[channel], phys_channel_cdr_table[channel]);
#endif

	return true;
}

void lrmac_suspend_physical(void){
	for(int i=0; i<8; i++){
		if(phys_corresponds_channel[i] != NULL)
			phys_corresponds_channel[i]->stop();
	}
}

void lrmac_send_packet(lrmac_packet_t *pkt){
	lrphys *phys = phys_corresponds_channel[pkt->channel];

	phys->packet_begin();
	phys->transmit(pkt->payload, pkt->payload_size);
	phys->packet_end();
	phys->set_mode_receive_it(0);
}


void lrmac_apply_setting(uint8_t channel, lrmac_phys_setting_t *phys_settings){
	phys_corresponds_channel[channel]->set_syncword(LRWGW_SYNCWORD);

	phys_corresponds_channel[channel]->set_frequency(phys_settings->freq);
	phys_corresponds_channel[channel]->set_txpower(phys_settings->powe);
	phys_corresponds_channel[channel]->set_spreadingfactor(phys_settings->sf);
	phys_corresponds_channel[channel]->set_bandwidth(phys_settings->bw);
	phys_corresponds_channel[channel]->set_codingrate4(phys_settings->codr);
//	phys_corresponds_channel[channel]->set_preamblelength(phys_settings->prea);
//	(phys_settings->crc)? phys_corresponds_channel[channel]->enable_crc():
//						  phys_corresponds_channel[channel]->disable_crc();
//	(phys_settings->iiq)? phys_corresponds_channel[channel]->enable_invertIQ():
//						  phys_corresponds_channel[channel]->disable_invertIQ();
}

void lrmac_restore_default_setting(uint8_t channel){
	lrmac_apply_setting(channel, &phys_settings_table[channel]);
}

uint8_t lrmac_get_channel_by_freq(long freq){
	uint8_t channel = 0;

	for(;channel<8; channel++){
		if(freq == phys_channel_freq_table[channel]) break;
	}

	return channel;
}


static void lrmac_phys_event_handler(void *arg, lrphys_eventid_t id, uint8_t len){
	lrphys *phys = (lrphys *)arg;
	lrmac_packet_t *pkt = NULL;


	pkt = (lrmac_packet_t *)malloc(sizeof(lrmac_packet_t));
	if(pkt == NULL) {
		LOG_ERROR(TAG, "Memory exhausted, malloc fail at %s -> %d", __FUNCTION__, __LINE__);
		return;
	}
	memset((void *)pkt, 0, sizeof(lrmac_packet_t));

	uint8_t channel = lrmac_get_phys_channel(phys);

	pkt->channel = channel;
	pkt->payload_size = len;

	if(id == LRPHYS_RECEIVE_COMPLETED && len > 0){
		pkt->payload = (uint8_t *)malloc(len+1);
		phys->receive((char *)pkt->payload);
		pkt->payload[len] = 0;
	}
	else if(id == LRPHYS_ERROR_CRC){
		pkt->payload = NULL;
		LOG_ERROR(TAG, "Phys received packet with invalid CRC");
	}
	else{
		pkt->payload = NULL;
	}
	pkt->eventid = id;

	BaseType_t ret = xQueueSendFromISR(*pqueue, &pkt, NULL);
	if(ret == pdTRUE)
		LOG_EVENT(TAG, "Channel %d receive uplink data", channel);
	else
		LOG_ERROR(TAG, "Error queue full at %s -> %d", __FUNCTION__, __LINE__);
}

static uint8_t lrmac_get_phys_channel(lrphys *phys){
	uint8_t channel = 0;

	for(;channel<8; channel++){
		if(phys == phys_corresponds_channel[channel]) break;
	}

	return channel;
}











