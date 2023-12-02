/*
 * log.h
 *
 *  Created on: Nov 6, 2023
 *      Author: anh
 */

#ifndef LOG_LOG_H_
#define LOG_LOG_H_

#include "stdio.h"
#include "stdarg.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CONFIG_LOG_MONITOR_LEVEL_SHORT 1
#define CONFIG_LOG_MONITOR_TICK        1

typedef enum {
	SIMP_BLACK = 0,
	SIMP_RED,
	SIMP_GREEN,
	SIMP_YELLOW,
	SIMP_BLUE,
	SIMP_PURPLE,
	SIMP_CYAN,
	SIMP_WHITE,

	BOLD_BLACK = 8,
	BOLD_RED,
	BOLD_GREEN,
	BOLD_YELLOW,
	BOLD_BLUE,
	BOLD_PURPLE,
	BOLD_CYAN,
	BOLD_WHITE,

	ITALIC_BLACK = 16,
	ITALIC_RED,
	ITALIC_GREEN,
	ITALIC_YELLOW,
	ITALIC_BLUE,
	ITALIC_PURPLE,
	ITALIC_CYAN,
	ITALIC_WHITE,

	BCKGRN_BLACK = 24,
	BCKGRN_RED,
	BCKGRN_GREEN,
	BCKGRN_YELLOW,
	BCKGRN_BLUE,
	BCKGRN_PURPLE,
	BCKGRN_CYAN,
	BCKGRN_WHITE,
} log_type_t;

#define LOG_MESS(__plog__, __tag__, __mess__) __plog__(__tag__, "%s[%d]>>> %s", __FUNCTION__, __LINE__, __mess__);

void log_monitor_init(void (*PrintString_Function)(char*));

void log_monitor_set_log(char *func, log_type_t log_type);

void LOG(log_type_t log_type, const char *tag, const char *format, ...);

void LOG_INFO(const char *tag, const char *format, ...);
void LOG_WARN(const char *tag, const char *format, ...);
void LOG_ERROR(const char *tag, const char *format, ...);
void LOG_DEBUG(const char *tag, const char *format, ...);
void LOG_EVENT(const char *tag, const char *format, ...);
void LOG_MEM(const char *tag, const char *format, ...);
void LOG_RET(const char *tag, const char *format, ...);

#ifdef __cplusplus
}
#endif

#endif /* LOG_LOG_H_ */
