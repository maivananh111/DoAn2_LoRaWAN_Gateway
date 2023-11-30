/*
 * Dev_Inf.c
 *
 *  Created on: Nov 29, 2023
 *      Author: anh
 */


#include "Dev_Inf.h"
#include "quadspi.h"

/* This structure containes information used by ST-LINK Utility to program and erase the device */
#if defined (__ICCARM__)
__root struct StorageInfo const StorageInfo  =  {
#else
struct StorageInfo const StorageInfo  =  {
#endif
   "XMC25QH64C_LRWGWBoard_H750",			  		// Device Name + EVAL Borad name
   NOR_FLASH,                   					// Device Type
   QSPI_BASE,                						// Device Start Address
   MEMORY_FLASH_SIZE,         						// Device Size in Bytes (2MBytes)
   MEMORY_PAGE_SIZE,                    			// Programming Page Size 256Bytes
   0xFF,                       						// Initial Content of Erased Memory
   MEMORY_SECTOR_NUM, MEMORY_SECTOR_SIZE,     	    // Sector Num ,Sector Size
   0x00000000, 0x00000000,
};
