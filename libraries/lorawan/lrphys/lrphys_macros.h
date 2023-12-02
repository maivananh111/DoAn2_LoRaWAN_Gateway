/*
 * lora_phys_reg.h
 *
 *  Created on: Nov 20, 2023
 *      Author: anh
 */

#ifndef LORAWAN_LRPHYS_LRPHYS_MACROS_H_
#define LORAWAN_LRPHYS_LRPHYS_MACROS_H_


/** Group: Register.
 * LoRa Physical register.
 */
#define LRPHYS_REG_FIFO                 0x00
#define LRPHYS_REG_OP_MODE              0x01
#define LRPHYS_REG_FRF_MSB              0x06
#define LRPHYS_REG_FRF_MID              0x07
#define LRPHYS_REG_FRF_LSB              0x08
#define LRPHYS_REG_PA_CONFIG            0x09
#define LRPHYS_REG_OCP                  0x0b
#define LRPHYS_REG_LNA                  0x0c
#define LRPHYS_REG_FIFO_ADDR_PTR        0x0d
#define LRPHYS_REG_FIFO_TX_BASE_ADDR    0x0e
#define LRPHYS_REG_FIFO_RX_BASE_ADDR    0x0f
#define LRPHYS_REG_FIFO_RX_CURRENT_ADDR 0x10
#define LRPHYS_REG_IRQ_FLAGS            0x12
#define LRPHYS_REG_RX_NB_BYTES          0x13
#define LRPHYS_REG_PKT_SNR_VALUE        0x19
#define LRPHYS_REG_PKT_RSSI_VALUE       0x1a
#define LRPHYS_REG_RSSI_VALUE           0x1b
#define LRPHYS_REG_MODEM_CONFIG_1       0x1d
#define LRPHYS_REG_MODEM_CONFIG_2       0x1e
#define LRPHYS_REG_PREAMBLE_MSB         0x20
#define LRPHYS_REG_PREAMBLE_LSB         0x21
#define LRPHYS_REG_PAYLOAD_LENGTH       0x22
#define LRPHYS_REG_MODEM_CONFIG_3       0x26
#define LRPHYS_REG_FREQ_ERROR_MSB       0x28
#define LRPHYS_REG_FREQ_ERROR_MID       0x29
#define LRPHYS_REG_FREQ_ERROR_LSB       0x2a
#define LRPHYS_REG_RSSI_WIDEBAND        0x2c
#define LRPHYS_REG_DETECTION_OPTIMIZE   0x31
#define LRPHYS_REG_INVERTIQ             0x33
#define LRPHYS_REG_DETECTION_THRESHOLD  0x37
#define LRPHYS_REG_SYNC_WORD            0x39
#define LRPHYS_REG_INVERTIQ2            0x3b
#define LRPHYS_REG_DIO_MAPPING_1        0x40
#define LRPHYS_REG_VERSION              0x42
#define LRPHYS_REG_PA_DAC               0x4d


/** Group: Mode.
 * LoRa Physical mode.
 */
#define LRPHYS_MODE_LONG_RANGE_MODE     0x80
#define LRPHYS_MODE_SLEEP               0x00
#define LRPHYS_MODE_STDBY               0x01
#define LRPHYS_MODE_TX                  0x03
#define LRPHYS_MODE_RX_CONTINUOUS       0x05
#define LRPHYS_MODE_RX_SINGLE           0x06

/** Group: PA boost.
 * LoRa Physical pa boost.
 */
#define LRPHYS_PA_BOOST                 0x80

/** Group: IRQ mask.
 * LoRa Physical irq mask.
 */
#define LRPHYS_IRQ_TX_DONE_MASK           0x08
#define LRPHYS_IRQ_PAYLOAD_CRC_ERROR_MASK 0x20
#define LRPHYS_IRQ_RX_DONE_MASK           0x40

/** Group: Specification.
 * LoRa Physical specification.
 */
#define LRPHYS_RF_MID_BAND_THRESHOLD      525E6
#define LRPHYS_RSSI_OFFSET_HF_PORT        157
#define LRPHYS_RSSI_OFFSET_LF_PORT        164
#define LRPHYS_MAX_PKT_LENGTH                       255

/** Group: RF output.
 * LoRa Physical RF output.
 */
#define LRPHYS_PA_OUTPUT_RFO_PIN          0
#define LRPHYS_PA_OUTPUT_PA_BOOST_PIN     1


#endif /* LORAWAN_LRPHYS_LRPHYS_MACROS_H_ */
