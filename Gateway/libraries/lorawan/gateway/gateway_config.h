/*
 * gateway_config.h
 *
 *  Created on: Nov 21, 2023
 *      Author: anh
 */

#ifndef LORAWAN_GATEWAY_GATEWAY_CONFIG_H_
#define LORAWAN_GATEWAY_GATEWAY_CONFIG_H_

#define LRWGW_MAC_DEBUG           1
#define LRWGW_WAN_DEBUG           1
#define LRWGW_UDP_DEBUG           1

#define LRWGW_SYNCWORD 			  0x34

#define LRWGW_CH0_SF  			  10
#define LRWGW_CH0_BW  			  125E3
#define LRWGW_CH0_CDR 			  5

#define LRWGW_CH1_SF  			  10
#define LRWGW_CH1_BW  			  125E3
#define LRWGW_CH1_CDR 			  5

#define LRWGW_CH2_SF  			  10
#define LRWGW_CH2_BW  			  125E3
#define LRWGW_CH2_CDR 			  5

#define LRWGW_CH3_SF  			  10
#define LRWGW_CH3_BW  			  125E3
#define LRWGW_CH3_CDR 			  5

#define LRWGW_CH4_SF  			  10
#define LRWGW_CH4_BW  			  125E3
#define LRWGW_CH4_CDR 			  5

#define LRWGW_CH5_SF  			  10
#define LRWGW_CH5_BW  			  125E3
#define LRWGW_CH5_CDR 			  5

#define LRWGW_CH6_SF  			  10
#define LRWGW_CH6_BW  			  125E3
#define LRWGW_CH6_CDR 			  5

#define LRWGW_CH7_SF  			  10
#define LRWGW_CH7_BW  			  125E3
#define LRWGW_CH7_CDR 			  5

#define LRWGW_STAT_INTERVAL       60U
#define LRWGW_KEEP_ALIVE          15U

#define LRWGW_DEFAULT_ID          0x123456789ABCDEF0

#define LRWGW_DEFAULT_PLATFORM    "STM32"
#define LRWGW_DEFAULT_DESCRIPTION "Dual channel LoRaWAN gateway"

#define LRWGW_DEFAULT_TTN_SERVER  "au1.cloud.thethings.network"
#define LRWGW_DEFAULT_NTP_SERVER  "asia.pool.ntp.org"
#define LRWGW_DEFAULT_TTN_PORT    1700
#define LRWGW_DEFAULT_NTP_PORT    123

#define LRWGW_DEFAULT_PROTO_VER   UDPSEM_PROTOVER_2

#define LRWGW_PHYS_RXPKT_QUEUE_SIZE 10
#define LRWGW_PHYS_TXPKT_QUEUE_SIZE 10

#define LRWGW_TIME_UTC_OFFSET_SEC 	7*3600U
#define LRWGW_BUFFER_SIZE 			512U
#define LRWGW_HEADER_LENGTH 		12U

#define LRWGW_FREQ_PLANS_AS923
#define LRWGW_FREQ_PLANS_EU433
#define LRWGW_FREQ_PLANS_AU915

#ifdef LRWGW_FREQ_PLANS_AS923
#define LRWGW_FREQ_MIN 923.2
#define LRWGW_FREQ_MAX 924.8
#endif
#define LRWGW_POWER_MIN 2
#define LRWGW_POWER_MAX 20


#endif /* LORAWAN_GATEWAY_GATEWAY_CONFIG_H_ */
