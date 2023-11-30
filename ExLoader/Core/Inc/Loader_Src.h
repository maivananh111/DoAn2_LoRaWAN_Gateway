/*
 * Loader_Src.h
 *
 *  Created on: Nov 29, 2023
 *      Author: anh
 */

#ifndef INC_LOADER_SRC_H_
#define INC_LOADER_SRC_H_

#include "stm32h7xx.h"


/* Private function prototypes -----------------------------------------------*/
int Init (void);
int Write (uint32_t Address, uint32_t Size, uint8_t* buffer);
int Read (uint32_t Address, uint32_t Size, uint8_t* Buffer);
int SectorErase (uint32_t EraseStartAddress ,uint32_t EraseEndAddress);
uint64_t Verify(uint32_t MemoryAddr, uint32_t RAMBufferAddr, uint32_t Size, uint32_t missalignement) ;

#endif /* INC_LOADER_SRC_H_ */
