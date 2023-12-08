/*
 * lrphys.cpp
 *
 *  Created on: Mar 26, 2023
 *      Author: anh
 */

#include "lorawan/lrphys/lrphys.h"

lrphys::lrphys(void) {
}

bool lrphys::initialize(lrphys_hwconfig_t *conf) {
	_conf = conf;

	HAL_GPIO_WritePin(_conf->cs_port, _conf->cs_pin, GPIO_PIN_SET);
	HAL_GPIO_WritePin(_conf->rst_port, _conf->rst_pin, GPIO_PIN_RESET);
	HAL_Delay(50);
	HAL_GPIO_WritePin(_conf->rst_port, _conf->rst_pin, GPIO_PIN_SET);
	HAL_Delay(50);

	uint8_t version = readRegister(LRPHYS_REG_VERSION);
	if (version != 0x12)
		return false;

	_freq = 433E6;
	sleep();
	set_frequency(_freq);
	writeRegister(LRPHYS_REG_FIFO_TX_BASE_ADDR, 0);
	writeRegister(LRPHYS_REG_FIFO_RX_BASE_ADDR, 0);
	writeRegister(LRPHYS_REG_LNA, readRegister(LRPHYS_REG_LNA) | 0x03);
	writeRegister(LRPHYS_REG_MODEM_CONFIG_3, 0x04);
	set_txpower(20, LRPHYS_PA_OUTPUT_PA_BOOST_PIN);
	enable_crc();
	idle();
	set_spreadingfactor(7);
	set_bandwidth(125E3);
	set_codingrate4(4);

	return true;
}

void lrphys::stop(void){
	idle();
	sleep();

	HAL_GPIO_WritePin(_conf->cs_port, _conf->cs_pin, GPIO_PIN_SET);
}

void lrphys::register_event_handler(lrphys_evtcb_f event_handler_function,
		void *parameter) {
	_event_handler = event_handler_function;
	_event_parameter = parameter;
}

int16_t lrphys::rssi(void) {
	return (readRegister(LRPHYS_REG_RSSI_VALUE)
			- (_freq < LRPHYS_RF_MID_BAND_THRESHOLD ?
					LRPHYS_RSSI_OFFSET_LF_PORT : LRPHYS_RSSI_OFFSET_HF_PORT));
}

void lrphys::set_mode_receive_it(uint8_t size) {
	if (_event_handler != NULL)
		writeRegister(LRPHYS_REG_DIO_MAPPING_1, 0x00); // DIO0 => RXDONE

	if (size > 0) {
		implicitHeaderMode();

		writeRegister(LRPHYS_REG_PAYLOAD_LENGTH, size & 0xff);
	} else {
		explicitHeaderMode();
	}

	writeRegister(LRPHYS_REG_OP_MODE,
			LRPHYS_MODE_LONG_RANGE_MODE | LRPHYS_MODE_RX_CONTINUOUS);
}

bool lrphys::packet_begin(bool implicitHeader) {
	if (is_transmitting())
		return false;

	idle();

	if (implicitHeader)
		implicitHeaderMode();
	else
		explicitHeaderMode();

	writeRegister(LRPHYS_REG_FIFO_ADDR_PTR, 0);
	writeRegister(LRPHYS_REG_PAYLOAD_LENGTH, 0);

	return true;
}

bool lrphys::packet_end(bool async) {
	if (async && (_event_handler != NULL))
		writeRegister(LRPHYS_REG_DIO_MAPPING_1, 0x40);

	writeRegister(LRPHYS_REG_OP_MODE,
			LRPHYS_MODE_LONG_RANGE_MODE | LRPHYS_MODE_TX);

	if (!async) {
		while ((readRegister(LRPHYS_REG_IRQ_FLAGS) & LRPHYS_IRQ_TX_DONE_MASK)
				== 0)
			;
		writeRegister(LRPHYS_REG_IRQ_FLAGS, LRPHYS_IRQ_TX_DONE_MASK);
	}

	return true;
}

bool lrphys::is_transmitting(void) {
	if ((readRegister(LRPHYS_REG_OP_MODE) & LRPHYS_MODE_TX) == LRPHYS_MODE_TX)
		return true;

	if (readRegister(LRPHYS_REG_IRQ_FLAGS) & LRPHYS_IRQ_TX_DONE_MASK)
		writeRegister(LRPHYS_REG_IRQ_FLAGS, LRPHYS_IRQ_TX_DONE_MASK);

	return false;
}

uint8_t lrphys::packet_parse(uint8_t size) {
	uint8_t packetLength = 0;
	uint8_t irqFlags = readRegister(LRPHYS_REG_IRQ_FLAGS);

	if (size > 0) {
		implicitHeaderMode();
		writeRegister(LRPHYS_REG_PAYLOAD_LENGTH, size & 0xff);
	} else
		explicitHeaderMode();

	writeRegister(LRPHYS_REG_IRQ_FLAGS, irqFlags);

	if ((irqFlags & LRPHYS_IRQ_RX_DONE_MASK)
			&& (irqFlags & LRPHYS_IRQ_PAYLOAD_CRC_ERROR_MASK) == 0) {
		_packetIndex = 0;

		if (_implicitHeaderMode)
			packetLength = readRegister(LRPHYS_REG_PAYLOAD_LENGTH);
		else
			packetLength = readRegister(LRPHYS_REG_RX_NB_BYTES);

		writeRegister(LRPHYS_REG_FIFO_ADDR_PTR,
				readRegister(LRPHYS_REG_FIFO_RX_CURRENT_ADDR));

		idle();
	} else if (irqFlags & LRPHYS_IRQ_PAYLOAD_CRC_ERROR_MASK) {
		if (_event_handler)
			_event_handler(_event_parameter, LRPHYS_ERROR_CRC, 0);
	} else if (readRegister(LRPHYS_REG_OP_MODE)
			!= (LRPHYS_MODE_LONG_RANGE_MODE | LRPHYS_MODE_RX_SINGLE)) {
		writeRegister(LRPHYS_REG_FIFO_ADDR_PTR, 0);

		writeRegister(LRPHYS_REG_OP_MODE,
				LRPHYS_MODE_LONG_RANGE_MODE | LRPHYS_MODE_RX_SINGLE);
	}

	return packetLength;
}

int lrphys::packet_rssi(void) {
	return (int) (readRegister(LRPHYS_REG_PKT_RSSI_VALUE)
			- (_freq < LRPHYS_RF_MID_BAND_THRESHOLD ?
					LRPHYS_RSSI_OFFSET_LF_PORT : LRPHYS_RSSI_OFFSET_HF_PORT));
}

float lrphys::packet_snr(void) {
	return ((int8_t) readRegister(LRPHYS_REG_PKT_SNR_VALUE)) * 0.25;
}

long lrphys::packet_freq_error(void) {
	int32_t freqError = 0;
	freqError = static_cast<int32_t>(readRegister(LRPHYS_REG_FREQ_ERROR_MSB)
			& 0b111);
	freqError <<= 8L;
	freqError += static_cast<int32_t>(readRegister(LRPHYS_REG_FREQ_ERROR_MID));
	freqError <<= 8L;
	freqError += static_cast<int32_t>(readRegister(LRPHYS_REG_FREQ_ERROR_LSB));

	if (readRegister(LRPHYS_REG_FREQ_ERROR_MSB) & 0b1000)
		freqError -= 524288;

	const float fXtal = 32E6;
	const float fError = ((static_cast<float>(freqError) * (1L << 24)) / fXtal)
			* (get_bandwidth() / 500000.0f); // p. 37

	return static_cast<long>(fError);
}

size_t lrphys::transmit(uint8_t byte) {
	return transmit(&byte, sizeof(byte));
}

size_t lrphys::transmit(const uint8_t *buffer, size_t size) {
	int currentLength = readRegister(LRPHYS_REG_PAYLOAD_LENGTH);

	if ((currentLength + size) > LRPHYS_MAX_PKT_LENGTH)
		size = LRPHYS_MAX_PKT_LENGTH - currentLength;

	for (size_t i = 0; i < size; i++)
		writeRegister(LRPHYS_REG_FIFO, buffer[i]);

	writeRegister(LRPHYS_REG_PAYLOAD_LENGTH, currentLength + size);

	return size;
}

uint8_t lrphys::available(void) {
	return (readRegister(LRPHYS_REG_RX_NB_BYTES) - _packetIndex);
}

uint8_t lrphys::receive(char *buffer) {
	uint8_t i = 0;
	while (available()) {
		_packetIndex++;

		buffer[i++] = readRegister(LRPHYS_REG_FIFO);
	}
	return _packetIndex;
}

uint8_t lrphys::peek(void) {
	if (!available())
		return -1;

	int currentAddress = readRegister(LRPHYS_REG_FIFO_ADDR_PTR);

	uint8_t b = readRegister(LRPHYS_REG_FIFO);

	writeRegister(LRPHYS_REG_FIFO_ADDR_PTR, currentAddress);

	return b;
}

void lrphys::idle(void) {
	writeRegister(LRPHYS_REG_OP_MODE,
			LRPHYS_MODE_LONG_RANGE_MODE | LRPHYS_MODE_STDBY);
}

void lrphys::sleep(void) {
	writeRegister(LRPHYS_REG_OP_MODE,
			LRPHYS_MODE_LONG_RANGE_MODE | LRPHYS_MODE_SLEEP);
}

void lrphys::set_txpower(uint8_t level, uint8_t outputPin) {
	if (LRPHYS_PA_OUTPUT_RFO_PIN == outputPin) {
		if (level < 0)
			level = 0;
		else if (level > 14)
			level = 14;

		writeRegister(LRPHYS_REG_PA_CONFIG, 0x70 | level);
	} else {
		if (level > 17) {
			if (level > 20)
				level = 20;
			level -= 3;

			writeRegister(LRPHYS_REG_PA_DAC, 0x87);
			set_ocp(140);
		} else {
			if (level < 2)
				level = 2;
			writeRegister(LRPHYS_REG_PA_DAC, 0x84);
			set_ocp(100);
		}

		writeRegister(LRPHYS_REG_PA_CONFIG, LRPHYS_PA_BOOST | (level - 2));
	}
}

void lrphys::set_frequency(long frequency) {
	uint64_t frf;

	_freq = frequency;
	frf = ((uint64_t) _freq << 19) / 32000000;

	writeRegister(LRPHYS_REG_FRF_MSB, (uint8_t) (frf >> 16));
	writeRegister(LRPHYS_REG_FRF_MID, (uint8_t) (frf >> 8));
	writeRegister(LRPHYS_REG_FRF_LSB, (uint8_t) (frf >> 0));
}

uint8_t lrphys::get_spreadingfactor(void) {
	return readRegister(LRPHYS_REG_MODEM_CONFIG_2) >> 4;
}

void lrphys::set_spreadingfactor(uint8_t sf) {
	if (sf < 6)
		sf = 6;
	else if (sf > 12)
		sf = 12;

	if (sf == 6) {
		writeRegister(LRPHYS_REG_DETECTION_OPTIMIZE, 0xc5);
		writeRegister(LRPHYS_REG_DETECTION_THRESHOLD, 0x0c);
	} else {
		writeRegister(LRPHYS_REG_DETECTION_OPTIMIZE, 0xc3);
		writeRegister(LRPHYS_REG_DETECTION_THRESHOLD, 0x0a);
	}

	writeRegister(LRPHYS_REG_MODEM_CONFIG_2,
			(readRegister(LRPHYS_REG_MODEM_CONFIG_2) & 0x0f)
					| ((sf << 4) & 0xf0));
	set_LDO_flag();
}

long lrphys::get_bandwidth(void) {
	uint8_t bw = (readRegister(LRPHYS_REG_MODEM_CONFIG_1) >> 4);

	switch (bw) {
	case 0:
		return 7.8E3;
	case 1:
		return 10.4E3;
	case 2:
		return 15.6E3;
	case 3:
		return 20.8E3;
	case 4:
		return 31.25E3;
	case 5:
		return 41.7E3;
	case 6:
		return 62.5E3;
	case 7:
		return 125E3;
	case 8:
		return 250E3;
	case 9:
		return 500E3;
	}

	return -1;
}

void lrphys::set_bandwidth(long sbw) {
	int bw;

	if (sbw <= 7.8E3)
		bw = 0;
	else if (sbw <= 10.4E3)
		bw = 1;
	else if (sbw <= 15.6E3)
		bw = 2;
	else if (sbw <= 20.8E3)
		bw = 3;
	else if (sbw <= 31.25E3)
		bw = 4;
	else if (sbw <= 41.7E3)
		bw = 5;
	else if (sbw <= 62.5E3)
		bw = 6;
	else if (sbw <= 125E3)
		bw = 7;
	else if (sbw <= 250E3)
		bw = 8;
	else
		/*if (sbw <= 250E3)*/bw = 9;

	writeRegister(LRPHYS_REG_MODEM_CONFIG_1,
			(readRegister(LRPHYS_REG_MODEM_CONFIG_1) & 0x0f) | (bw << 4));
	set_LDO_flag();
}

void lrphys::set_LDO_flag(void) {
	long symbolDuration = 1000
			/ (get_bandwidth() / (1L << get_spreadingfactor()));
	bool ldoOn = symbolDuration > 16;
	uint8_t config3 = readRegister(LRPHYS_REG_MODEM_CONFIG_3);
	if (ldoOn)
		config3 |= (1 << 3);
	else
		config3 &= ~(1 << 3);

	writeRegister(LRPHYS_REG_MODEM_CONFIG_3, config3);
}

void lrphys::set_codingrate4(uint8_t denominator) {
	if (denominator < 5)
		denominator = 5;
	else if (denominator > 8)
		denominator = 8;

	uint8_t cr = denominator - 4;

	writeRegister(LRPHYS_REG_MODEM_CONFIG_1,
			(readRegister(LRPHYS_REG_MODEM_CONFIG_1) & 0xf1) | (cr << 1));
}

void lrphys::set_preamblelength(long length) {
	writeRegister(LRPHYS_REG_PREAMBLE_MSB, (uint8_t) (length >> 8));
	writeRegister(LRPHYS_REG_PREAMBLE_LSB, (uint8_t) (length >> 0));
}

void lrphys::set_syncword(uint8_t sw) {
	writeRegister(LRPHYS_REG_SYNC_WORD, sw);
}

void lrphys::enable_crc(void) {
	writeRegister(LRPHYS_REG_MODEM_CONFIG_2,
			readRegister(LRPHYS_REG_MODEM_CONFIG_2) | 0x04);
}

void lrphys::disable_crc(void) {
	writeRegister(LRPHYS_REG_MODEM_CONFIG_2,
			readRegister(LRPHYS_REG_MODEM_CONFIG_2) & 0xfb);
}

void lrphys::enable_invertIQ(void) {
	writeRegister(LRPHYS_REG_INVERTIQ, 0x66);
	writeRegister(LRPHYS_REG_INVERTIQ2, 0x19);
}

void lrphys::disable_invertIQ(void) {
	writeRegister(LRPHYS_REG_INVERTIQ, 0x27);
	writeRegister(LRPHYS_REG_INVERTIQ2, 0x1d);
}

void lrphys::set_ocp(uint8_t mA) {
	uint8_t ocpTrim = 27;

	if (mA <= 120)
		ocpTrim = (mA - 45) / 5;
	else if (mA <= 240)
		ocpTrim = (mA + 30) / 10;

	writeRegister(LRPHYS_REG_OCP, 0x20 | (0x1F & ocpTrim));
}

void lrphys::set_gain(uint8_t gain) {
	if (gain > 6)
		gain = 6;

	idle();

	if (gain == 0)
		writeRegister(LRPHYS_REG_MODEM_CONFIG_3, 0x04);
	else {
		writeRegister(LRPHYS_REG_MODEM_CONFIG_3, 0x00);

		writeRegister(LRPHYS_REG_LNA, 0x03);

		writeRegister(LRPHYS_REG_LNA,
				readRegister(LRPHYS_REG_LNA) | (gain << 5));
	}
}

void lrphys::explicitHeaderMode(void) {
	_implicitHeaderMode = 0;

	writeRegister(LRPHYS_REG_MODEM_CONFIG_1,
			readRegister(LRPHYS_REG_MODEM_CONFIG_1) & 0xfe);
}

void lrphys::implicitHeaderMode(void) {
	_implicitHeaderMode = 1;

	writeRegister(LRPHYS_REG_MODEM_CONFIG_1,
			readRegister(LRPHYS_REG_MODEM_CONFIG_1) | 0x01);
}

void lrphys::IRQHandler(void) {
	uint8_t irqFlags = readRegister(LRPHYS_REG_IRQ_FLAGS);

	writeRegister(LRPHYS_REG_IRQ_FLAGS, irqFlags);

	if ((irqFlags & LRPHYS_IRQ_PAYLOAD_CRC_ERROR_MASK) == 0) {
		if ((irqFlags & LRPHYS_IRQ_RX_DONE_MASK) != 0) {
			_packetIndex = 0;

			uint8_t packetLength =
					_implicitHeaderMode ?
							readRegister(LRPHYS_REG_PAYLOAD_LENGTH) :
							readRegister(LRPHYS_REG_RX_NB_BYTES);

			writeRegister(LRPHYS_REG_FIFO_ADDR_PTR,
					readRegister(LRPHYS_REG_FIFO_RX_CURRENT_ADDR));

			if (_event_handler != NULL)
				_event_handler(_event_parameter, LRPHYS_RECEIVE_COMPLETED,
						packetLength);

		} else if ((irqFlags & LRPHYS_IRQ_TX_DONE_MASK) != 0) {
			if (_event_handler)
				_event_handler(_event_parameter, LRPHYS_TRANSMIT_COMPLETED, 0);
		}
	} else {
		if (_event_handler)
			_event_handler(_event_parameter, LRPHYS_ERROR_CRC, 0);
	}
}

uint8_t lrphys::readRegister(uint8_t address) {
	return singleTransfer(address & 0x7f, 0x00);
}

void lrphys::writeRegister(uint8_t address, uint8_t value) {
	singleTransfer(address | 0x80, value);
}

uint8_t lrphys::singleTransfer(uint8_t address, uint8_t value) {
	uint8_t response, txdt;

	HAL_GPIO_WritePin(_conf->cs_port, _conf->cs_pin, GPIO_PIN_RESET);

	txdt = address;
	HAL_SPI_Transmit(_conf->spi, (uint8_t*) (&txdt), 1, 1000);
	txdt = value;
	HAL_SPI_TransmitReceive(_conf->spi, (uint8_t*) (&txdt),
			(uint8_t*) (&response), 1, 1000);

	HAL_GPIO_WritePin(_conf->cs_port, _conf->cs_pin, GPIO_PIN_SET);

	return response;
}

