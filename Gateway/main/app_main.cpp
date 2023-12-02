/*
 * app_main.cpp
 *
 *  Created on: Nov 6, 2023
 *      Author: anh
 */
#include "main.h"
#include "stdio.h"
#include "string.h"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include "log/log.h"
#include "sysinfo/sysinfo.h"
#include "ethconn/ethconn.h"

/**
 *
 */
void ethernet_link_event_handler(ethernet_event_t event);

void app_main(void){
	ethernet_register_event_handler(ethernet_link_event_handler);
	ethernet_init_link(5000);

	while(1){
//		LOG_MEM("MEM", "Free heap size: %lubytes", dev_get_free_heap_size());

		HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);

		vTaskDelay(1000);
	}
}


/**
 *
 */
void ethernet_link_event_handler(ethernet_event_t event){
	static const char *TAG = "Ethernet LINK";

	switch(event){
		case ETHERNET_EVENT_LINKUP:
			LOG_INFO(TAG, "Ethernet event linked up");
		break;
		case ETHERNET_EVENT_LINKDOWN:
			LOG_INFO(TAG, "Ethernet event linked down");
		break;
		case ETHERNET_EVENT_GOTIP:
			LOG_INFO(TAG, "Ethernet event got IP");
		break;
	}
}
