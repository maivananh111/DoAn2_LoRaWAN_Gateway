/*
 * app_main.cpp
 *
 *  Created on: Nov 6, 2023
 *      Author: anh
 */

#include "gpio.h"
#include "spi.h"

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


#define GATEWAY_ID   0xACAE1F2C324C44F5

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

void ethernet_link_event_handler(ethernet_event_t event);
void lorawan_gateway_event_handler(lorawan_gateway_t *pgtw, lorawan_gateway_event_t event, void *param);
void task_lorawan_gateway(void *);

lorawan_gateway_t gateway = {
	.server_info = {
		.ttn_server = TTN_SERVER,
		.port = TTN_PORT,
		.udpver = UDPSEM_PROTOVER_2,
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
};


/**
 *
 */
void app_main(void){
	ethernet_register_event_handler(ethernet_link_event_handler);
	ethernet_init_link(5000);

	while(1){
		vTaskDelay(500);
	}
}


/**
 *
 */
void task_lorawan(void *){
	lorawan_gateway_initialize(&gateway);

	lrmac_link_physical(&lora_ch0, &lora_ch0_hw, 0);

	lorawan_gateway_register_event_handler(&gateway, lorawan_gateway_event_handler, NULL);

	lorawan_gateway_start(&gateway);


	while(1){
		lorawan_gateway_process(&gateway);
		vTaskDelay(10);
	}
}
/**
 *
 */
void lorawan_gateway_event_handler(lorawan_gateway_t *pgtw, lorawan_gateway_event_t event, void *param){

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
		break;
		case ETHERNET_EVENT_GOTIP:
			LOG_EVENT(TAG, "Ethernet event got IP");
			xTaskCreate(task_lorawan, "task_lorawan", 65536/4, NULL, 10, NULL);
		break;
	}
}

extern "C" void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin){
	if(GPIO_Pin == ETH_LINK_INT_Pin){
		LOG_EVENT("Ethernet", "Ethernet link interrupted");
	}
	else if(GPIO_Pin == LORA1_IO0_Pin){
		lora_ch0.IRQHandler();
	}
}
