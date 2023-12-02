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

#include "log/log.h"


static const char *TAG = "LoRaMAC";



static QueueHandle_t queue_phys_pkt;

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
	info->rssi = info->phys->packet_rssi();
	info->snr  = info->phys->packet_snr();
}


void lrmac_initialize(void){
	queue_phys_pkt = xQueueCreate(LRWGW_PHYS_RXPKT_QUEUE_SIZE, sizeof(lrmac_macpkt_t *));
}

bool lrmac_link_physical(lrphys *phys, lrphys_hwconfig_t *hwconf, uint8_t channel){
	if(channel < 0) channel = 0;
	if(channel > 7) channel = 7;

	if(!phys->initialize(hwconf)) return false;
	phys->register_event_handler(lrmac_phys_event_handler, (void *)phys);

	phys->set_syncword(LRWGW_SYNCWORD);
	phys->set_frequency(phys_channel_freq_table[channel]);
	phys->set_spreadingfactor(phys_channel_sf_table[channel]);
	phys->set_bandwidth(phys_channel_bw_table[channel]);
	phys->set_codingrate4(phys_channel_cdr_table[channel]);

	phys->set_mode_receive_it(0);

	phys_corresponds_channel[channel] = phys;

#if LRWGW_MAC_DEBUG
	LOG_INFO(TAG, "Add channel %d[freq: %luHz, sf: %d, bw: %lu, codr: 4/%d]",
			channel, phys_channel_freq_table[channel], phys_channel_sf_table[channel], phys_channel_bw_table[channel], phys_channel_cdr_table[channel]);
#endif

	return true;
}

void lrwgw_mac_forward_downlink(lrmac_macpkt_t *pkt){
	lrphys *phys = phys_corresponds_channel[pkt->channel];

	phys->packet_begin();
	phys->transmit(pkt->payload, pkt->payload_size);
	phys->packet_end();
}


static void lrmac_phys_event_handler(void *arg, lrphys_eventid_t id, uint8_t len){
	lrphys *phys = (lrphys *)arg;
	lrmac_macpkt_t *pkt = NULL;

	pkt = (lrmac_macpkt_t *)malloc(sizeof(lrmac_macpkt_t));
	if(pkt == NULL) return;
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
#if LRWGW_MAC_DEBUG
		LOG_ERROR(TAG, "Phys received packet with invalid CRC");
#endif
	}
	else{
		pkt->payload = NULL;
	}
	pkt->eventid = id;
	BaseType_t ret = (xPortIsInsideInterrupt())?
			xQueueSendFromISR(queue_phys_pkt, &pkt, NULL):
			xQueueSend(queue_phys_pkt, &pkt, 10);
#if LRWGW_MAC_DEBUG
	if(ret == pdTRUE)
		LOG_EVENT(TAG, "Channel %d receive uplink data", channel);
	else
		LOG_ERROR(TAG, "Error queue full");
#endif
}

static uint8_t lrmac_get_phys_channel(lrphys *phys){
	uint8_t channel = 0;

	for(;channel<8; channel++){
		if(phys == phys_corresponds_channel[channel]) break;
	}

	return channel;
}

uint8_t lrmac_rxpacket_available(lrmac_macpkt_t *ppkt){
	return (xPortIsInsideInterrupt())?
			(uint8_t)xQueueReceiveFromISR(queue_phys_pkt, &ppkt, NULL):
			(uint8_t)xQueueReceive(queue_phys_pkt, &ppkt, 10);
}












