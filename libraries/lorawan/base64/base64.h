/*
 * base64.h
 *
 *  Created on: Nov 15, 2023
 *      Author: anh
 */

#ifndef LORAWAN_BASE64_BASE64_H_
#define LORAWAN_BASE64_BASE64_H_



#include <stdlib.h>
#include "stdint.h"

#ifdef __cplusplus
extern "C"{
#endif



uint16_t base64_int(uint16_t ch);

uint16_t base64e_size(uint16_t in_size);
uint16_t base64d_size(uint16_t in_size);


uint16_t base64_encode(const uint8_t* in, uint16_t in_len, uint8_t* out);
uint16_t base64_decode(const uint8_t* in, uint16_t in_len, uint8_t* out);



#ifdef __cplusplus
}
#endif


#endif /* LORAWAN_BASE64_BASE64_H_ */
