/*
 * sysinfo.cpp
 *
 *  Created on: Nov 6, 2023
 *      Author: anh
 */


#include "sysinfo.h"
#include "malloc.h"



#define TICKS_PER_SECOND 1000


extern char _end;
extern char _sdata;
extern char _estack;
extern char _Min_Stack_Size;

static char *ramstart = &_sdata;
static char *ramend = &_estack;
static char *minSP = (char*)(ramend - &_Min_Stack_Size);

extern "C" char *sbrk(int i);

static __IO uint32_t _mem_total_ram_size = CONFIG_MEM_IRAM_CAPACITY * 1024; // 128*1024
static __IO uint32_t _mem_total_ram_use  = 0U;


cpu_info_t cpu_info;


uint32_t dev_get_revid(void){
	return ((DBGMCU -> IDCODE) >> 16U);
}

uint32_t dev_get_devid(void){
	return ((DBGMCU -> IDCODE) & 0x0FFFU);
}

void dev_get_uniqueid(uint32_t *puniid){
	puniid[0] = *(__IO uint32_t *)(REG_UNIQUE_ID);
	puniid[1] = *(__IO uint32_t *)(REG_UNIQUE_ID + 4);
	puniid[2] = *(__IO uint32_t *)(REG_UNIQUE_ID + 8);
}

uint32_t dev_get_flashsize(void){
	return (*(__IO uint16_t*)REG_FLASH_INFO);
}

void dev_get_cpu_info(cpu_info_t *info){
	info->device_name = (char *)DEVICE_NAME;
	info->device_cpu = (char *)CONFIG_CPU_CORE;
	info->device_id = ((DBGMCU -> IDCODE) & 0x0FFFU);
	info->revision_id = ((DBGMCU -> IDCODE) >> 16U);
	info->flashsize_Kb = (*(__IO uint16_t*)REG_FLASH_INFO);
	info->ramsize_kb = CONFIG_MEM_IRAM_CAPACITY;
	dev_get_uniqueid(info->unique_id);
}



mem_info_t dev_get_memory_info(void){
	mem_info_t mem;
	char *heapend = (char*)sbrk(0);
	char * stack_ptr = (char*)__get_MSP();
	struct mallinfo mi = mallinfo();

	mem.free_ram = ((stack_ptr < minSP) ? stack_ptr : minSP) - heapend + mi.fordblks;
	mem.heap_ram_used = mi.uordblks;
	mem.prog_ram_used = &_end - ramstart;
	mem.stack_ram_used = ramend - stack_ptr;
	mem.total_free_ram = mi.fordblks;

	return mem;
}

uint32_t dev_get_free_heap_size(void){
	char *heapend = (char*)sbrk(0);
	char * stack_ptr = (char*)__get_MSP();
	struct mallinfo mi = mallinfo();

	return ((stack_ptr < minSP) ? stack_ptr : minSP) - heapend + mi.fordblks;
}

uint32_t dev_get_used_heap_size(void){
	struct mallinfo mi = mallinfo();

	return mi.uordblks;
}

