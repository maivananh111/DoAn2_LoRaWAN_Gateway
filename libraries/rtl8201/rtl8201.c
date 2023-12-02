/*
 * rtl8201.c
 *
 *  Created on: Nov 1, 2023
 *      Author: anh
 */

#include "rtl8201/rtl8201.h"
#include "rtl8201/rtl8201_macros.h"

static const char *TAG = "RTL8201 PHYS";

extern void LOG_INFO(const char *tag, const char *format, ...);
extern void LOG_ERROR(const char *tag, const char *format, ...);

static void RTL8201_Page_Select(rtl8201_Object_t *pObj, uint32_t page) {
	pObj->IO.WriteReg(pObj->DevAddr, RTL8201_PAGESEL, page);
}

int32_t RTL8201_RegisterBusIO(rtl8201_Object_t *pObj, rtl8201_IOCtx_t *ioctx) {
	if (!pObj || !ioctx->ReadReg || !ioctx->WriteReg || !ioctx->GetTick) {
		return RTL8201_STATUS_ERROR;
	}

	pObj->IO.Init = ioctx->Init;
	pObj->IO.DeInit = ioctx->DeInit;
	pObj->IO.ReadReg = ioctx->ReadReg;
	pObj->IO.WriteReg = ioctx->WriteReg;
	pObj->IO.GetTick = ioctx->GetTick;

	return RTL8201_STATUS_OK;
}

int32_t RTL8201_Init(rtl8201_Object_t *pObj) {
	uint32_t tickstart = 0, regvalue = 0, addr = 0;
	int32_t status = RTL8201_STATUS_OK;

	if (pObj->Is_Initialized == 0) {
		if (pObj->IO.Init != 0)
			pObj->IO.Init();

		pObj->DevAddr = RTL8201_MAX_DEV_ADDR + 1;

		RTL8201_Page_Select(pObj, RTL8201_REGPAGE_0);
		for (addr = 0; addr <= RTL8201_MAX_DEV_ADDR; addr++) {
			if (pObj->IO.ReadReg(addr, RTL8201_MACR, &regvalue) < 0) {
				status = RTL8201_STATUS_READ_ERROR;
				continue;
			}
			if ((regvalue & RTL8201_SMR_PHY_ADDR) == addr) {
				pObj->DevAddr = addr;
				LOG_INFO(TAG, "Phys address: 0x%04x", pObj->DevAddr);
				status = RTL8201_STATUS_OK;
				break;
			}
		}

		if (pObj->DevAddr > RTL8201_MAX_DEV_ADDR)
			status = RTL8201_STATUS_ADDRESS_ERROR;

		if (status == RTL8201_STATUS_OK) {
			if (pObj->IO.WriteReg(pObj->DevAddr, RTL8201_BCR,
					RTL8201_BCR_SOFT_RESET) >= 0) {
				if (pObj->IO.ReadReg(pObj->DevAddr, RTL8201_BCR, &regvalue)
						>= 0) {
					tickstart = pObj->IO.GetTick();
					while (regvalue & RTL8201_BCR_SOFT_RESET) {
						if ((pObj->IO.GetTick() - tickstart)
								<= RTL8201_SW_RESET_TO) {
							if (pObj->IO.ReadReg(pObj->DevAddr, RTL8201_BCR,
									&regvalue) < 0) {
								status = RTL8201_STATUS_READ_ERROR;
								break;
							}
						} else {
							status = RTL8201_STATUS_RESET_TIMEOUT;
							break;
						}
					}
				} else
					status = RTL8201_STATUS_READ_ERROR;
			} else
				status = RTL8201_STATUS_WRITE_ERROR;
		}
	}

	if (status == RTL8201_STATUS_OK) {
		tickstart = pObj->IO.GetTick();
		while ((pObj->IO.GetTick() - tickstart) <= RTL8201_INIT_TO)
			;
		if (pObj->IO.ReadReg(pObj->DevAddr, RTL8201_PHYI1R, &regvalue) >= 0)
			LOG_INFO(TAG, "Phys identifier: 0x%04x", regvalue);

		RTL8201_Page_Select(pObj, RTL8201_REGPAGE_7);

		pObj->IO.ReadReg(pObj->DevAddr, RTL8201_IWLF, &regvalue);
		pObj->IO.WriteReg(pObj->DevAddr, RTL8201_IWLF, (regvalue | 0x0008));
		pObj->IO.WriteReg(pObj->DevAddr, RTL8201_CLSR, 0x38);

		RTL8201_Page_Select(pObj, RTL8201_REGPAGE_0);

		pObj->Is_Initialized = 1;
	}

	return status;
}

int32_t RTL8201_DeInit(rtl8201_Object_t *pObj) {
	if (pObj->Is_Initialized) {
		if (pObj->IO.DeInit != 0) {
			if (pObj->IO.DeInit() < 0)
				return RTL8201_STATUS_ERROR;
		}

		pObj->Is_Initialized = 0;
	}

	return RTL8201_STATUS_OK;
}

int32_t RTL8201_DisablePowerDownMode(rtl8201_Object_t *pObj) {
	uint32_t readval = 0;
	int32_t status = RTL8201_STATUS_OK;

	if (pObj->IO.ReadReg(pObj->DevAddr, RTL8201_BCR, &readval) >= 0) {
		readval &= ~RTL8201_BCR_POWER_DOWN;
		if (pObj->IO.WriteReg(pObj->DevAddr, RTL8201_BCR, readval) < 0)
			status = RTL8201_STATUS_WRITE_ERROR;
	} else
		status = RTL8201_STATUS_READ_ERROR;

	return status;
}

int32_t RTL8201_EnablePowerDownMode(rtl8201_Object_t *pObj) {
	uint32_t readval = 0;
	int32_t status = RTL8201_STATUS_OK;

	if (pObj->IO.ReadReg(pObj->DevAddr, RTL8201_BCR, &readval) >= 0) {
		readval |= RTL8201_BCR_POWER_DOWN;
		if (pObj->IO.WriteReg(pObj->DevAddr, RTL8201_BCR, readval) < 0)
			status = RTL8201_STATUS_WRITE_ERROR;
	} else
		status = RTL8201_STATUS_READ_ERROR;

	return status;
}

int32_t RTL8201_StartAutoNego(rtl8201_Object_t *pObj) {
	uint32_t readval = 0;
	int32_t status = RTL8201_STATUS_OK;

	if (pObj->IO.ReadReg(pObj->DevAddr, RTL8201_BCR, &readval) >= 0) {
		readval |= RTL8201_BCR_AUTONEGO_EN;
		if (pObj->IO.WriteReg(pObj->DevAddr, RTL8201_BCR, readval) < 0)
			status = RTL8201_STATUS_WRITE_ERROR;
	} else
		status = RTL8201_STATUS_READ_ERROR;

	return status;
}

int32_t RTL8201_GetLinkState(rtl8201_Object_t *pObj) {
	uint32_t readval = 0;

	if (pObj->IO.ReadReg(pObj->DevAddr, RTL8201_BSR, &readval) < 0)
		return RTL8201_STATUS_READ_ERROR;

	if (pObj->IO.ReadReg(pObj->DevAddr, RTL8201_BSR, &readval) < 0)
		return RTL8201_STATUS_READ_ERROR;

	if ((readval & RTL8201_BSR_LINK_STATUS) == 0)
		return RTL8201_STATUS_LINK_DOWN;

	if (pObj->IO.ReadReg(pObj->DevAddr, RTL8201_BCR, &readval) < 0)
		return RTL8201_STATUS_READ_ERROR;

	if ((readval & RTL8201_BCR_AUTONEGO_EN) != RTL8201_BCR_AUTONEGO_EN) {
		if (((readval & RTL8201_BCR_SPEED_SELECT) == RTL8201_BCR_SPEED_SELECT)
				&& ((readval & RTL8201_BCR_DUPLEX_MODE)
						== RTL8201_BCR_DUPLEX_MODE))
			return RTL8201_STATUS_100MBITS_FULLDUPLEX;
		else if ((readval & RTL8201_BCR_SPEED_SELECT)
				== RTL8201_BCR_SPEED_SELECT)
			return RTL8201_STATUS_100MBITS_HALFDUPLEX;
		else if ((readval & RTL8201_BCR_DUPLEX_MODE) == RTL8201_BCR_DUPLEX_MODE)
			return RTL8201_STATUS_10MBITS_FULLDUPLEX;
		else
			return RTL8201_STATUS_10MBITS_HALFDUPLEX;
	} else {
		if (pObj->IO.ReadReg(pObj->DevAddr, RTL8201_PHYSCSR, &readval) < 0)
			return RTL8201_STATUS_READ_ERROR;

		if ((readval & RTL8201_PHYSCSR_AUTONEGO_DONE) == 0)
			return RTL8201_STATUS_AUTONEGO_NOTDONE;

		if ((readval & RTL8201_PHYSCSR_HCDSPEEDMASK)
				== RTL8201_PHYSCSR_100BTX_FD)
			return RTL8201_STATUS_100MBITS_FULLDUPLEX;
		else if ((readval & RTL8201_PHYSCSR_HCDSPEEDMASK)
				== RTL8201_PHYSCSR_100BTX_HD)
			return RTL8201_STATUS_100MBITS_HALFDUPLEX;
		else if ((readval & RTL8201_PHYSCSR_HCDSPEEDMASK)
				== RTL8201_PHYSCSR_10BT_FD)
			return RTL8201_STATUS_10MBITS_FULLDUPLEX;
		else
			return RTL8201_STATUS_10MBITS_HALFDUPLEX;
	}

	return RTL8201_STATUS_LINK_DOWN;
}

int32_t RTL8201_SetLinkState(rtl8201_Object_t *pObj, uint32_t LinkState) {
	uint32_t bcrvalue = 0;
	int32_t status = RTL8201_STATUS_OK;

	if (pObj->IO.ReadReg(pObj->DevAddr, RTL8201_BCR, &bcrvalue) >= 0) {
		bcrvalue &= ~(RTL8201_BCR_AUTONEGO_EN | RTL8201_BCR_SPEED_SELECT
				| RTL8201_BCR_DUPLEX_MODE);

		if (LinkState == RTL8201_STATUS_100MBITS_FULLDUPLEX)
			bcrvalue |= (RTL8201_BCR_SPEED_SELECT | RTL8201_BCR_DUPLEX_MODE);
		else if (LinkState == RTL8201_STATUS_100MBITS_HALFDUPLEX)
			bcrvalue |= RTL8201_BCR_SPEED_SELECT;
		else if (LinkState == RTL8201_STATUS_10MBITS_FULLDUPLEX)
			bcrvalue |= RTL8201_BCR_DUPLEX_MODE;
		else
			status = RTL8201_STATUS_ERROR;
	} else
		status = RTL8201_STATUS_READ_ERROR;

	if (status == RTL8201_STATUS_OK) {
		if (pObj->IO.WriteReg(pObj->DevAddr, RTL8201_BCR, bcrvalue) < 0)
			status = RTL8201_STATUS_WRITE_ERROR;
	}

	return status;
}

int32_t RTL8201_EnableLoopbackMode(rtl8201_Object_t *pObj) {
	uint32_t readval = 0;
	int32_t status = RTL8201_STATUS_OK;

	if (pObj->IO.ReadReg(pObj->DevAddr, RTL8201_BCR, &readval) >= 0) {
		readval |= RTL8201_BCR_LOOPBACK;

		if (pObj->IO.WriteReg(pObj->DevAddr, RTL8201_BCR, readval) < 0)
			status = RTL8201_STATUS_WRITE_ERROR;
	} else
		status = RTL8201_STATUS_READ_ERROR;

	return status;
}

int32_t RTL8201_DisableLoopbackMode(rtl8201_Object_t *pObj) {
	uint32_t readval = 0;
	int32_t status = RTL8201_STATUS_OK;

	if (pObj->IO.ReadReg(pObj->DevAddr, RTL8201_BCR, &readval) >= 0) {
		readval &= ~RTL8201_BCR_LOOPBACK;
		if (pObj->IO.WriteReg(pObj->DevAddr, RTL8201_BCR, readval) < 0)
			status = RTL8201_STATUS_WRITE_ERROR;
	} else
		status = RTL8201_STATUS_READ_ERROR;

	return status;
}

int32_t RTL8201_EnableIT(rtl8201_Object_t *pObj, uint32_t Interrupt) {
	uint32_t readval = 0;
	int32_t status = RTL8201_STATUS_OK;

	if (pObj->IO.ReadReg(pObj->DevAddr, RTL8201_IMR, &readval) >= 0) {
		readval |= Interrupt;
		if (pObj->IO.WriteReg(pObj->DevAddr, RTL8201_IMR, readval) < 0)
			status = RTL8201_STATUS_WRITE_ERROR;
	} else
		status = RTL8201_STATUS_READ_ERROR;

	return status;
}

int32_t RTL8201_DisableIT(rtl8201_Object_t *pObj, uint32_t Interrupt) {
	uint32_t readval = 0;
	int32_t status = RTL8201_STATUS_OK;

	if (pObj->IO.ReadReg(pObj->DevAddr, RTL8201_IMR, &readval) >= 0) {
		readval &= ~Interrupt;
		if (pObj->IO.WriteReg(pObj->DevAddr, RTL8201_IMR, readval) < 0)
			status = RTL8201_STATUS_WRITE_ERROR;
	} else
		status = RTL8201_STATUS_READ_ERROR;

	return status;
}

int32_t RTL8201_ClearIT(rtl8201_Object_t *pObj, uint32_t Interrupt) {
	uint32_t readval = 0;
	int32_t status = RTL8201_STATUS_OK;

	if (pObj->IO.ReadReg(pObj->DevAddr, RTL8201_ISFR, &readval) < 0)
		status = RTL8201_STATUS_READ_ERROR;

	return status;
}

int32_t RTL8201_GetITStatus(rtl8201_Object_t *pObj, uint32_t Interrupt) {
	uint32_t readval = 0;
	int32_t status = 0;

	if (pObj->IO.ReadReg(pObj->DevAddr, RTL8201_ISFR, &readval) >= 0)
		status = ((readval & Interrupt) == Interrupt);
	else
		status = RTL8201_STATUS_READ_ERROR;

	return status;
}

/**
 * @}
 */

/**
 * @}
 */

/**
 * @}
 */

/**
 * @}
 */

