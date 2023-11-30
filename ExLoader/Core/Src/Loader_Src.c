/*
 * Loader_Src.c
 *
 *  Created on: Nov 29, 2023
 *      Author: anh
 */

#include "quadspi.h"
#include "main.h"
#include "gpio.h"
#include "usart.h"
#include "log.h"
#include "string.h"


#define LOADER_OK   0x1
#define LOADER_FAIL 0x0

static const char *TAG = "External loader";

extern void SystemClock_Config(void);
static void log_out(char *log);

static void log_out(char *log) {
	HAL_UART_Transmit(&huart8, (uint8_t*) log, strlen(log), 1000);
}
/**
 * @brief  System initialization.
 * @param  None
 * @retval  LOADER_OK = 1   : Operation succeeded
 * @retval  LOADER_FAIL = 0 : Operation failed
 */
int Init(void) {
    *(uint32_t*)0xE000EDF0 = 0xA05F0000; //enable interrupts in debug

    SystemInit();

    SCB->VTOR = 0x24000000 | 0x200;

    __set_PRIMASK(0); //enable interrupts

    HAL_Init();

    SystemClock_Config();

    MX_GPIO_Init();
    MX_UART8_Init();

	log_monitor_init(log_out);
	log_out((char*) "\r\n*+*+*+*+*+*+*+*+*+*+*+*+*+*+*+*+*+*+*External loader for STM32H7 device series*+*+*+*+*+*+*+*+*+*+*+*+*+*+*+*+*+*+*");

	LOG_INFO(TAG, "%s", __FUNCTION__);
    __HAL_RCC_QSPI_FORCE_RESET();  //completely reset peripheral
    __HAL_RCC_QSPI_RELEASE_RESET();

    MX_QUADSPI_Init();

    if (CSP_QUADSPI_Init() != HAL_OK) {
    	LOG_ERROR(TAG, "Fail to initialize QUADSPI");
        __set_PRIMASK(1); //disable interrupts
        return LOADER_FAIL;
    }

//    if (CSP_QSPI_EnableMemoryMappedMode() != HAL_OK) {
//    	LOG_ERROR(TAG, "Fail to set memory mapped mode");
//        __set_PRIMASK(1); //disable interrupts
//        return LOADER_FAIL;
//    }

    /*Trigger read access before HAL_QSPI_Abort() otherwise abort functionality gets stuck*/
//    uint32_t a = *(uint32_t*) 0x90000000;
//    a++;

    __set_PRIMASK(1); //disable interrupts
    return LOADER_OK;
}

/**
 * @brief   Program memory.
 * @param   Address: page address
 * @param   Size   : size of data
 * @param   buffer : pointer to data buffer
 * @retval  LOADER_OK = 1       : Operation succeeded
 * @retval  LOADER_FAIL = 0 : Operation failed
 */
int Write(uint32_t Address, uint32_t Size, uint8_t* buffer) {
//	int ret = MassErase();
//	if(ret == 0) return LOADER_FAIL;
	LOG_INFO(TAG, "%s %lu bytes to 0x%08x", __FUNCTION__, Size, Address);
    __set_PRIMASK(0); //enable interrupts

    if (HAL_QSPI_Abort(&hqspi) != HAL_OK) {
    	LOG_ERROR(TAG, "Fail to abort QUADSPI");
        __set_PRIMASK(1); //disable interrupts
        return LOADER_FAIL;
    }

    if (CSP_QSPI_WriteMemory((uint8_t*) buffer, (Address & (0x0fffffff)), Size) != HAL_OK) {
    	LOG_ERROR(TAG, "Fail to write to memory, address = 0x%08x, size = %lu", Address, Size);
        __set_PRIMASK(1); //disable interrupts
        return LOADER_FAIL;
    }
    LOG_INFO(TAG, "Written to memory, address = 0x%08x, size = %lu", Address, Size);

    __set_PRIMASK(1); //disable interrupts
    return LOADER_OK;
}

/**
 *
 * @param Address
 * @param Size
 * @param Buffer
 * @return
 */
int Read (uint32_t Address, uint32_t Size, uint8_t* Buffer){
	LOG_WARN(TAG, "%s %lu bytes form 0x%08x ", __FUNCTION__, Size, Address);
    __set_PRIMASK(0); //enable interrupts

    if (CSP_QSPI_EnableMemoryMappedMode() != HAL_OK) {
    	LOG_ERROR(TAG, "Fail to set memory mapped mode");
        __set_PRIMASK(1); //disable interrupts
        return LOADER_FAIL;
    }

	int i = 0;
	for (i=0; i < Size;i++){
		*(uint8_t *)Buffer++ = *(uint8_t*)Address++;
	}

    __set_PRIMASK(1); //disable interrupts

    return LOADER_OK;
}

/**
 * @brief   Sector erase.
 * @param   EraseStartAddress :  erase start address
 * @param   EraseEndAddress   :  erase end address
 * @retval  LOADER_OK = 1       : Operation succeeded
 * @retval  LOADER_FAIL = 0 : Operation failed
 */
int SectorErase(uint32_t EraseStartAddress, uint32_t EraseEndAddress) {
	LOG_INFO(TAG, "%s from 0x%08x to 0x%08x", __FUNCTION__, EraseStartAddress, EraseEndAddress);
    __set_PRIMASK(0); //enable interrupts

    if (HAL_QSPI_Abort(&hqspi) != HAL_OK) {
    	LOG_ERROR(TAG, "Fail to abort QUADSPI");
        __set_PRIMASK(1); //disable interrupts
        return LOADER_FAIL;
    }

    if (CSP_QSPI_EraseSector(EraseStartAddress, EraseEndAddress) != HAL_OK) {
    	 LOG_ERROR(TAG, "Fail to erase sector, start address = 0x%08x, end address = 0x%08x", EraseStartAddress, EraseEndAddress);
        __set_PRIMASK(1); //disable interrupts
        return LOADER_FAIL;
    }
    LOG_INFO(TAG, "Erase sector successful, start address = 0x%08x, end address = 0x%08x", EraseStartAddress, EraseEndAddress);

    __set_PRIMASK(1); //disable interrupts
    return LOADER_OK;
}

/**
 * Description :
 * Mass erase of external flash area
 * Optional command - delete in case usage of mass erase is not planed
 * Inputs    :
 *      none
 * outputs   :
 *     none
 * Note: Optional for all types of device
 */
int MassErase(void) {
	LOG_INFO(TAG, "%s", __FUNCTION__);
    __set_PRIMASK(0); //enable interrupts

    if (HAL_QSPI_Abort(&hqspi) != HAL_OK) {
    	LOG_ERROR(TAG, "Failed to abort QUADSPI");
        __set_PRIMASK(1); //disable interrupts
        return LOADER_FAIL;
    }

    if (CSP_QSPI_Erase_Chip() != HAL_OK) {
    	LOG_ERROR(TAG, "Failed to erase chip");
        __set_PRIMASK(1); //disable interrupts
        return LOADER_FAIL;
    }
    LOG_INFO(TAG, "Erase chip successful");

    HAL_Delay(1000);

    __set_PRIMASK(1); //disable interrupts

    return LOADER_OK;
}

/**
 * Description :
 * Calculates checksum value of the memory zone
 * Inputs    :
 *      StartAddress  : Flash start address
 *      Size          : Size (in WORD)
 *      InitVal       : Initial CRC value
 * outputs   :
 *     R0             : Checksum value
 * Note: Optional for all types of device
 */
uint32_t CheckSum(uint32_t StartAddress, uint32_t Size, uint32_t InitVal) {
	LOG_INFO(TAG, "%s", __FUNCTION__);
    __set_PRIMASK(0); //enable interrupts

    if (CSP_QSPI_EnableMemoryMappedMode() != HAL_OK) {
    	LOG_ERROR(TAG, "Fail to set memory mapped mode");
        __set_PRIMASK(1); //disable interrupts
        return LOADER_FAIL;
    }

    uint8_t missalignementAddress = StartAddress % 4;
    uint8_t missalignementSize = Size;
    int cnt;
    uint32_t Val;

    StartAddress -= StartAddress % 4;
    Size += (Size % 4 == 0) ? 0 : 4 - (Size % 4);

    for (cnt = 0; cnt < Size; cnt += 4) {
        Val = *(uint32_t*) StartAddress;
        if (missalignementAddress) {
            switch (missalignementAddress) {
                case 1:
                    InitVal += (uint8_t) (Val >> 8 & 0xff);
                    InitVal += (uint8_t) (Val >> 16 & 0xff);
                    InitVal += (uint8_t) (Val >> 24 & 0xff);
                    missalignementAddress -= 1;
                    break;
                case 2:
                    InitVal += (uint8_t) (Val >> 16 & 0xff);
                    InitVal += (uint8_t) (Val >> 24 & 0xff);
                    missalignementAddress -= 2;
                    break;
                case 3:
                    InitVal += (uint8_t) (Val >> 24 & 0xff);
                    missalignementAddress -= 3;
                    break;
            }
        } else if ((Size - missalignementSize) % 4 && (Size - cnt) <= 4) {
            switch (Size - missalignementSize) {
                case 1:
                    InitVal += (uint8_t) Val;
                    InitVal += (uint8_t) (Val >> 8 & 0xff);
                    InitVal += (uint8_t) (Val >> 16 & 0xff);
                    missalignementSize -= 1;
                    break;
                case 2:
                    InitVal += (uint8_t) Val;
                    InitVal += (uint8_t) (Val >> 8 & 0xff);
                    missalignementSize -= 2;
                    break;
                case 3:
                    InitVal += (uint8_t) Val;
                    missalignementSize -= 3;
                    break;
            }
        } else {
            InitVal += (uint8_t) Val;
            InitVal += (uint8_t) (Val >> 8 & 0xff);
            InitVal += (uint8_t) (Val >> 16 & 0xff);
            InitVal += (uint8_t) (Val >> 24 & 0xff);
        }
        StartAddress += 4;
    }
    __set_PRIMASK(1); //disable interrupts

    return (InitVal);
}

/**
 * Description :
 * Verify flash memory with RAM buffer and calculates checksum value of
 * the programmed memory
 * Inputs    :
 *      FlashAddr     : Flash address
 *      RAMBufferAddr : RAM buffer address
 *      Size          : Size (in WORD)
 *      InitVal       : Initial CRC value
 * outputs   :
 *     R0             : Operation failed (address of failure)
 *     R1             : Checksum value
 * Note: Optional for all types of device
 */
uint64_t Verify(uint32_t MemoryAddr, uint32_t RAMBufferAddr, uint32_t Size, uint32_t missalignement) {
	LOG_INFO(TAG, "%s", __FUNCTION__);
    __set_PRIMASK(0); //enable interrupts
    uint32_t VerifiedData = 0, InitVal = 0;
    uint64_t checksum;
    Size *= 4;

    if (CSP_QSPI_EnableMemoryMappedMode() != HAL_OK) {
    	LOG_ERROR(TAG, "Failed to set QUADSPI memory mapped mode, line %d", __LINE__);
        __set_PRIMASK(1); //disable interrupts
        return LOADER_FAIL;
    }

    checksum = CheckSum((uint32_t) MemoryAddr + (missalignement & 0xf),
                        Size - ((missalignement >> 16) & 0xF), InitVal);
    while (Size > VerifiedData) {
        if (*(uint8_t*) MemoryAddr++
            != *((uint8_t*) RAMBufferAddr + VerifiedData)) {
            __set_PRIMASK(1); //disable interrupts
            return ((checksum << 32) + (MemoryAddr + VerifiedData));
        }
        VerifiedData++;
    }

    __set_PRIMASK(1); //disable interrupts
    return (checksum << 32);
}
