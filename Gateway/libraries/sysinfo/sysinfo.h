/*
 * sysinfo.h
 *
 *  Created on: Nov 6, 2023
 *      Author: anh
 */

#ifndef SYSINFO_SYSINFO_H_
#define SYSINFO_SYSINFO_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "stdio.h"
#include "stm32h7xx_hal.h"

#if defined(STM32F0) || defined(STM32F3)
#define REG_FLASH_INFO 0x1FFFF7CC
#define REG_UNIQUE_ID  0x1FFFF7AC
#elif defined(STM32F1)
#define REG_FLASH_INFO 0x1FFFF7E0
#define REG_UNIQUE_ID  0x1FFFF7E8
#elif defined(STM32F2) || defined(STM32F4)
#define REG_FLASH_INFO 0x1FFF7A22
#define REG_UNIQUE_ID  0x1FFF7A10
#elif defined(STM32F7) || defined(STM32H7)
#define REG_FLASH_INFO 0x1FF0F442
#define REG_UNIQUE_ID  0x1FF0F420
#endif

#define DEVICE_NAME "STM32F407VGT6"
#define CONFIG_CPU_CORE "ARM Cortex-M4"
#define CONFIG_MEM_IRAM_CAPACITY 128

typedef struct memory_info {
	uint32_t heap_ram_used;
	uint32_t prog_ram_used;
	uint32_t stack_ram_used;
	uint32_t free_ram;
	uint32_t total_free_ram;
} mem_info_t;

typedef struct {
	char *device_name;
	char *device_cpu;
	uint32_t revision_id = 0;
	uint32_t device_id = 0;
	uint32_t unique_id[3] = { 0 };
	uint32_t flashsize_Kb = 0;
	uint32_t ramsize_kb = 0;
} cpu_info_t;

extern cpu_info_t cpu_info;

uint32_t dev_get_revid(void);
uint32_t dev_get_devid(void);
void dev_get_uniqueid(uint32_t *puniid);
uint32_t dev_get_flashsize(void);

void dev_get_cpu_info(cpu_info_t *info);

mem_info_t dev_get_memory_info(void);
uint32_t dev_get_free_heap_size(void);
uint32_t dev_get_used_heap_size(void);

void dev_cal_cpu_load_percent(void);
float dev_get_cpu_load_percent(void);
float dev_get_ram_used_percent(void);

#ifdef __cplusplus
}
#endif

#endif /* SYSINFO_SYSINFO_H_ */
