/*
 * rtl8201.h
 *
 *  Created on: Nov 6, 2023
 *      Author: anh
 */

#ifndef RTL8201_RTL8201_H_
#define RTL8201_RTL8201_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "stdio.h"
#include "stm32h7xx_hal.h"

typedef int32_t (*rtl8201_Init_Func)(void);
typedef int32_t (*rtl8201_DeInit_Func)(void);
typedef int32_t (*rtl8201_ReadReg_Func)(uint32_t, uint32_t, uint32_t*);
typedef int32_t (*rtl8201_WriteReg_Func)(uint32_t, uint32_t, uint32_t);
typedef int32_t (*rtl8201_GetTick_Func)(void);

typedef struct {
	rtl8201_Init_Func Init;
	rtl8201_DeInit_Func DeInit;
	rtl8201_WriteReg_Func WriteReg;
	rtl8201_ReadReg_Func ReadReg;
	rtl8201_GetTick_Func GetTick;
} rtl8201_IOCtx_t;

typedef struct {
	uint32_t DevAddr;
	uint32_t Is_Initialized;
	rtl8201_IOCtx_t IO;
	void *pData;
} rtl8201_Object_t;

int32_t RTL8201_RegisterBusIO(rtl8201_Object_t *pObj, rtl8201_IOCtx_t *ioctx);

int32_t RTL8201_Init(rtl8201_Object_t *pObj);
int32_t RTL8201_DeInit(rtl8201_Object_t *pObj);

int32_t RTL8201_EnablePowerDownMode(rtl8201_Object_t *pObj);
int32_t RTL8201_DisablePowerDownMode(rtl8201_Object_t *pObj);

int32_t RTL8201_StartAutoNego(rtl8201_Object_t *pObj);

int32_t RTL8201_GetLinkState(rtl8201_Object_t *pObj);
int32_t RTL8201_SetLinkState(rtl8201_Object_t *pObj, uint32_t LinkState);

int32_t RTL8201_EnableLoopbackMode(rtl8201_Object_t *pObj);
int32_t RTL8201_DisableLoopbackMode(rtl8201_Object_t *pObj);

int32_t RTL8201_EnableIT(rtl8201_Object_t *pObj, uint32_t Interrupt);
int32_t RTL8201_DisableIT(rtl8201_Object_t *pObj, uint32_t Interrupt);
int32_t RTL8201_ClearIT(rtl8201_Object_t *pObj, uint32_t Interrupt);
int32_t RTL8201_GetITStatus(rtl8201_Object_t *pObj, uint32_t Interrupt);

#ifdef __cplusplus
}
#endif

#endif /* RTL8201_RTL8201_H_ */
