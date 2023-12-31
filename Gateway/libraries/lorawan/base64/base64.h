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


/**
@brief Encode binary data in Base64 string (no padding)
@param in pointer to a table of binary data
@param size number of bytes to be encoded to base64
@param out pointer to a string where the function will output encoded data
@param max_len max length of the out string (including null char)
@return >=0 length of the resulting string (w/o null char), -1 for error
*/
int bin_to_b64_nopad(const uint8_t * in, int size, char * out, int max_len);

/**
@brief Decode Base64 string to binary data (no padding)
@param in string containing only base64 valid characters
@param size number of characters to be decoded from base64 (w/o null char)
@param out pointer to a data buffer where the function will output decoded data
@param out_max_len usable size of the output data buffer
@return >=0 number of bytes written to the data buffer, -1 for error
*/
int b64_to_bin_nopad(const char * in, int size, uint8_t * out, int max_len);

/* === derivative functions === */

/**
@brief Encode binary data in Base64 string (with added padding)
*/
int bin_to_b64(const uint8_t * in, int size, char * out, int max_len);

/**
@brief Decode Base64 string to binary data (remove padding if necessary)
*/
int b64_to_bin(const char * in, int size, uint8_t * out, int max_len);




#ifdef __cplusplus
}
#endif


#endif /* LORAWAN_BASE64_BASE64_H_ */
