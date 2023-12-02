/*
 * startup.cpp
 *
 *  Created on: Nov 6, 2023
 *      Author: anh
 */
#include "stdio.h"
#include "string.h"

#include "FreeRTOS.h"
#include "task.h"

#include "stm32f4xx_hal.h"

#include "log/log.h"

#define APP_MAIN_TASK_STACKSIZE_BYTE 4096

extern UART_HandleTypeDef huart6;

extern "C" int _write(int file, char *ptr, int len);
static void log_out(char *log);
static void app_main_task(void *param);
int edf_main_application(void);

static void app_main_task(void *param) {
	extern void app_main(void);
	app_main();

	vTaskDelete(NULL);
}

int edf_main_application(void) {
	log_monitor_init(log_out);
	log_out((char*) "\r\n\r\n");
	log_out((char*) "\r\n\r\n*+*+*+*+*+*+*+*+*+*+*+*+*+*+*+*+*+*+*Target starting*+*+*+*+*+*+*+*+*+*+*+*+*+*+*+*+*+*+*\r\n");

	BaseType_t app_start_status = xTaskCreate(app_main_task, "app_main_task",
			APP_MAIN_TASK_STACKSIZE_BYTE / 4, NULL, 1, NULL);
	if (app_start_status != pdTRUE)
		return 0;

	vTaskStartScheduler();

	return (int) app_start_status;
}

static void log_out(char *log) {
	HAL_UART_Transmit(&huart6, (uint8_t*) log, strlen(log), 1000);
}

extern "C" int _write(int file, char *ptr, int len) {
	HAL_UART_Transmit(&huart6, (uint8_t*) ptr, len, 1000);

	fflush(stdout);

	return len;
}

