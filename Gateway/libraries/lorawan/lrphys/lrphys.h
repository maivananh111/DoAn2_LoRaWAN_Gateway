/*
 * lrphys.h
 *
 *  Created on: Mar 26, 2023
 *      Author: anh
 */

#ifndef LORAWAN_LRPHYS_LRPHYS_H_
#define LORAWAN_LRPHYS_LRPHYS_H_


#ifdef __cplusplus
extern "C"{
#endif

#include "stm32h7xx_hal.h"
#include "lorawan/lrphys/lrphys_macros.h"





typedef enum{
	LRPHYS_TRANSMIT_COMPLETED,
	LRPHYS_RECEIVE_COMPLETED,
	LRPHYS_ERROR_CRC,
} lrphys_eventid_t;

typedef void(*lrphys_evtcb_f)(void *arg, lrphys_eventid_t id, uint8_t len);

typedef struct{
	/**
	 * Phys pin configure.
	 */
	SPI_HandleTypeDef *spi;
	GPIO_TypeDef *cs_port;
	uint16_t     cs_pin;
	GPIO_TypeDef *rst_port;
	uint16_t     rst_pin;
	GPIO_TypeDef *io0_port;
	uint16_t 	 io0_pin;
} lrphys_hwconfig_t;



class lrphys{
	public:
		lrphys(void);

		bool initialize(lrphys_hwconfig_t *conf = NULL);
		void stop(void);
		void register_event_handler(lrphys_evtcb_f event_handler_function = NULL, void *parameter = NULL);

		void set_mode_receive_it(uint8_t size);
		int16_t rssi(void);

		bool packet_begin(bool implicitHeader = false);
		bool packet_end(bool async = false);
		uint8_t packet_parse(uint8_t size = 0);
		int packet_rssi(void);
		float packet_snr(void);
		long packet_freq_error(void);

		size_t transmit(uint8_t byte);
		size_t transmit(const uint8_t *buffer, size_t size);

		uint8_t available(void);
		uint8_t receive(char *buffer);
		uint8_t peek(void);

		void idle(void);
		void sleep(void);

		void set_txpower(uint8_t level, uint8_t outputPin = LRPHYS_PA_OUTPUT_PA_BOOST_PIN);
		void set_frequency(long frequency);
		void set_spreadingfactor(uint8_t sf);
		void set_bandwidth(long sbw);
		void set_codingrate4(uint8_t denominator);
		void set_preamblelength(long length);
		void set_syncword(uint8_t sw);
		void set_ocp(uint8_t mA);
		void set_gain(uint8_t gain);

		void enable_crc(void);
		void disable_crc(void);
		void enable_invertIQ(void);
		void disable_invertIQ(void);

		void IRQHandler(void);


	protected:
		uint8_t readRegister(uint8_t address);
		void writeRegister(uint8_t address, uint8_t value);
		uint8_t singleTransfer(uint8_t address, uint8_t value);

	private:
		void explicitHeaderMode(void);
		void implicitHeaderMode(void);

		bool is_transmitting(void);

		uint8_t get_spreadingfactor();
		long get_bandwidth(void);

		void set_LDO_flag(void);

		lrphys_hwconfig_t *_conf;

		lrphys_evtcb_f    _event_handler = NULL;
		void 			  *_event_parameter = NULL;

		long			  _freq;
		int 		 	  _packetIndex = 0;
		int 			  _implicitHeaderMode = 0;

};



#ifdef __cplusplus
}
#endif

#endif /* LORAWAN_LRPHYS_LRPHYS_H_ */
