/*
 * app_main.cpp
 *
 *  Created on: Nov 6, 2023
 *      Author: anh
 */

#include "gpio.h"
#include "spi.h"
#include "rtc.h"

#include "stdio.h"
#include "string.h"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "log/log.h"
#include "sysinfo/sysinfo.h"

#include "ethconn/ethconn.h"
#include "lorawan/lrphys/lrphys.h"
#include "lorawan/lrmac/lrmac.h"
#include "lorawan/gateway/gateway.h"


/**
 *
 */


#define GATEWAY_ID   0xABCDEFFF01020304

#define NTP_SERVER "asia.pool.ntp.org"
#define TTN_SERVER "au1.cloud.thethings.network"
#define TTN_PORT   1700
/**
 *
 */


lrphys lora_ch0;
lrphys_hwconfig_t lora_ch0_hw = {
	.spi      = &hspi1,
	.cs_port  = LORA1_CS_GPIO_Port,
	.cs_pin   = LORA1_CS_Pin,
	.rst_port = LORA1_RST_GPIO_Port,
	.rst_pin  = LORA1_RST_Pin,
	.io0_port = LORA1_IO0_GPIO_Port,
	.io0_pin  = LORA1_IO0_Pin
};

lrphys lora_ch1;
lrphys_hwconfig_t lora_ch1_hw = {
	.spi      = &hspi4,
	.cs_port  = LORA2_CS_GPIO_Port,
	.cs_pin   = LORA2_CS_Pin,
	.rst_port = LORA2_RST_GPIO_Port,
	.rst_pin  = LORA2_RST_Pin,
	.io0_port = LORA2_IO0_GPIO_Port,
	.io0_pin  = LORA2_IO0_Pin
};

void ethernet_link_event_handler(ethernet_event_t event);
void lorawan_gateway_event_handler(lorawan_gateway_t *pgtw, lorawan_gateway_event_t event, void *param);

lorawan_gateway_t gateway = {
	.server_info = {
		.ttn_server = TTN_SERVER,
		.port       = TTN_PORT,
		.udpver     = UDPSEM_PROTOVER_2,
		.ntp_server = NTP_SERVER,
	},
	.gateway_info = {
		.id = GATEWAY_ID,
		.latitude  		  = 10.83911490301822,
		.longitude 		  = 106.7936735902942,
		.altitude         = 14,
		.platform         = "STM32",
		.mail			  = "anh.iot1708@gmail.com",
		.description      = "STM32 Gateway",
	},
	.stat_interval        = LRWGW_STAT_INTERVAL,
	.keepalive_interval   = LRWGW_KEEP_ALIVE,
};


/**
 *
 */
void app_main(void){
	ethernet_register_event_handler(ethernet_link_event_handler);
	ethernet_init_link(10000);

	while(1){
		vTaskDelay(1000);
	}
}

/**
 *
 */
void lorawan_gateway_event_handler(lorawan_gateway_t *pgtw, lorawan_gateway_event_t event, void *param){
	static const char *TAG = "LoRaWAN gateway event";

	HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
	switch(event){
		case LORAWAN_GATEWAY_CONNECT:
			LOG_WARN(TAG, "LORAWAN_GATEWAY_CONNECT");
		break;
		case LORAWAN_GATEWAY_DISCONNECT:
			LOG_WARN(TAG, "LORAWAN_GATEWAY_DISCONNECT");
		break;
		case LORAWAN_GATEWAY_EVENT_DOWNLINK:
			LOG_WARN(TAG, "LORAWAN_GATEWAY_EVENT_DOWNLINK");
		break;
		case LORAWAN_GATEWAY_EVENT_UPLINK:
			LOG_WARN(TAG, "LORAWAN_GATEWAY_EVENT_UPLINK");
		break;
	}

	HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
}


/**
 *
 */
void ethernet_link_event_handler(ethernet_event_t event){
	static const char *TAG = "Ethernet LINK";

	switch(event){
		case ETHERNET_EVENT_LINKUP:
			LOG_EVENT(TAG, "Ethernet event link up");
		break;

		case ETHERNET_EVENT_LINKDOWN:
			LOG_EVENT(TAG, "Ethernet event link down");

			lorawan_gateway_stop(&gateway);
			lrmac_suspend_physical();
		break;

		case ETHERNET_EVENT_DHCP_ERROR:
			LOG_EVENT(TAG, "Ethernet event DHCP error");

			vTaskDelay(1000);

			__NVIC_SystemReset();
		break;

		case ETHERNET_EVENT_GOTIP:
			LOG_EVENT(TAG, "Ethernet event got IP");
			HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
			lorawan_gateway_initialize(&gateway);

			lorawan_gateway_register_event_handler(&gateway, lorawan_gateway_event_handler, NULL);

			while(lorawan_gateway_start(&gateway) != ERR_OK){
				LOG_ERROR("LoRaWAN gateway", "Fail to start gateway...");
				vTaskDelay(1000);
			}

			lrmac_link_physical(&lora_ch0, &lora_ch0_hw, 0);
			lrmac_link_physical(&lora_ch1, &lora_ch1_hw, 1);

			HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);
		break;
	}
}

extern "C" void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin){
	if(GPIO_Pin == LORA1_IO0_Pin){
		lora_ch0.IRQHandler();
	}
	if(GPIO_Pin == LORA2_IO0_Pin){
		lora_ch1.IRQHandler();
	}
}
