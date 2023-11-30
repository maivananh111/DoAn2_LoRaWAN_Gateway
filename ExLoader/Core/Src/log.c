/*
 * log.cpp
 *
 *  Created on: Nov 6, 2023
 *      Author: anh
 */

#include "log.h"

#include "stm32h7xx_hal.h"

#include "string.h"
#include "stdlib.h"
#include "stdio.h"



static log_type_t logi = SIMP_GREEN;	// Information.
static log_type_t logw = SIMP_YELLOW;   // Warning.
static log_type_t loge = SIMP_RED;		// Error.
static log_type_t logd = SIMP_BLUE;		// Debug.
static log_type_t logm = SIMP_WHITE;	// Memory.
static log_type_t logv = SIMP_CYAN;	    // Parameter.
static log_type_t logr = SIMP_PURPLE;	// Result.

#if CONFIG_LOG_MONITOR_LEVEL_SHORT
static const char *log_level_str[] = {
	"I",
	"W",
	"E",
	"D",
	"M",
	"V",
	"R",
};
#else
static const char *log_level_str[] = {
	"INFOR ",
	"WARNN ",
	"ERROR ",
	"DEBUG ",
	"MEMORY",
	"EVENT ",
	"RESULT",
};
#endif

void (*plog)(char *str);
static const char *COLOR_END = "\033[0m";
static const char *LOG_COLOR[] = {
	"\033[0;30m",
	"\033[0;31m",
	"\033[0;32m",
	"\033[0;33m",
	"\033[0;34m",
	"\033[0;35m",
	"\033[0;36m",
	"\033[0;37m",

	// Bold
	"\033[1;30m",
	"\033[1;31m",
	"\033[1;32m",
	"\033[1;33m",
	"\033[1;34m",
	"\033[1;35m",
	"\033[1;36m",
	"\033[1;37m",

	// Italic
	"\033[4;30m",
	"\033[4;31m",
	"\033[4;32m",
	"\033[4;33m",
	"\033[4;34m",
	"\033[4;35m",
	"\033[4;36m",
	"\033[4;37m",

	// Background
	"\033[40m",
	"\033[41m",
	"\033[42m",
	"\033[43m",
	"\033[44m",
	"\033[45m",
	"\033[46m",
	"\033[47m",
};


/**
 * @fn void log_init(void(*)(char*))
 * @brief
 *
 * @pre
 * @post
 * @param PrintString_Function
 */
void log_monitor_init(void (*PrintString_Function)(char*)){
	plog = PrintString_Function;
}

/**
 * @fn void set_log(char*, log_type_t)
 * @brief
 *
 * @pre
 * @post
 * @param func
 * @param log_type
 */
void log_monitor_set_log(char *func, log_type_t log_type){
	if	   (strcmp(func, (char *)"INFOR")  == 0) logi = log_type;
	else if(strcmp(func, (char *)"WARNN")  == 0) logw = log_type;
	else if(strcmp(func, (char *)"ERROR")  == 0) loge = log_type;
	else if(strcmp(func, (char *)"DEBUG")  == 0) logd = log_type;
	else if(strcmp(func, (char *)"MEMORY") == 0) logm = log_type;
	else if(strcmp(func, (char *)"PARAM")  == 0) logv = log_type;
	else if(strcmp(func, (char *)"RESULT") == 0) logr = log_type;
	else LOG_ERROR("Parameter Error", "Unknown function %s.", func);
}

/**
 * @fn void LOG(log_type_t, const char*, const char*, ...)
 * @brief
 *
 * @pre
 * @post
 * @param log_type
 * @param tag
 * @param format
 */

void LOG(log_type_t log_type, const char *tag, const char *format, ...){
#if CONFIG_LOG_MONITOR_TICK
	uint32_t time = HAL_GetTick();
#endif
	char *Temp_buffer = NULL;
	va_list args;
	va_start(args, format);
	vasprintf(&Temp_buffer, format, args);
	va_end(args);

	char *Output_buffer;
#if CONFIG_LOG_MONITOR_TICK
	asprintf(&Output_buffer, "\r\n%s[%010lu] %s: %s%s", LOG_COLOR[log_type], time, tag, Temp_buffer, COLOR_END);
#else
	asprintf(&Output_buffer, "\r\n%s%s: %s%s", LOG_COLOR[log_type], tag, Temp_buffer, COLOR_END);
#endif

	plog(Output_buffer);
	free(Temp_buffer);
	free(Output_buffer);
}

/**
 * @fn void LOG_INFO(const char*, const char*, ...)
 * @brief
 *
 * @pre
 * @post
 * @param tag
 * @param format
 */
void LOG_INFO(const char *tag,  const char *format, ...){
#if CONFIG_LOG_MONITOR_TICK
	uint32_t time = HAL_GetTick();
#endif
	char *Temp_buffer = NULL;
	va_list args;
	va_start(args, format);
	vasprintf(&Temp_buffer, format, args);
	va_end(args);

	char *Output_buffer;
#if CONFIG_LOG_MONITOR_TICK
	asprintf(&Output_buffer, "\r\n%s%s [%lu] %s: %s%s", LOG_COLOR[logi], log_level_str[0], time, tag, Temp_buffer, COLOR_END);
#else
	asprintf(&Output_buffer, "\r\n%s%s %s: %s%s", LOG_COLOR[logi], log_level_str[0], tag, Temp_buffer, COLOR_END);
#endif

	plog(Output_buffer);
	free(Temp_buffer);
	free(Output_buffer);
}

/**
 * @fn void LOG_WARN(const char*, const char*, ...)
 * @brief
 *
 * @pre
 * @post
 * @param tag
 * @param format
 */
void LOG_WARN(const char *tag,  const char *format, ...){
#if CONFIG_LOG_MONITOR_TICK
	uint32_t time = HAL_GetTick();
#endif
	char *Temp_buffer = NULL;
	va_list args;
	va_start(args, format);
	vasprintf(&Temp_buffer, format, args);
	va_end(args);

	char *Output_buffer;
#if CONFIG_LOG_MONITOR_TICK
	asprintf(&Output_buffer, "\r\n%s%s [%lu] %s: %s%s", LOG_COLOR[logw], log_level_str[1], time, tag, Temp_buffer, COLOR_END);
#else
	asprintf(&Output_buffer, "\r\n%s%s %s: %s%s", LOG_COLOR[logw], log_level_str[1], tag, Temp_buffer, COLOR_END);
#endif

	plog(Output_buffer);
	free(Temp_buffer);
	free(Output_buffer);
}

/**
 * @fn void LOG_ERROR(const char*, const char*, ...)
 * @brief
 *
 * @pre
 * @post
 * @param tag
 * @param format
 */
void LOG_ERROR(const char *tag,  const char *format, ...){
#if CONFIG_LOG_MONITOR_TICK
	uint32_t time = HAL_GetTick();
#endif
	char *Temp_buffer = NULL;
	va_list args;
	va_start(args, format);
	vasprintf(&Temp_buffer, format, args);
	va_end(args);

	char *Output_buffer;
#if CONFIG_LOG_MONITOR_TICK
	asprintf(&Output_buffer, "\r\n%s%s [%lu] %s: %s%s", LOG_COLOR[loge], log_level_str[2], time, tag, Temp_buffer, COLOR_END);
#else
	asprintf(&Output_buffer, "\r\n%s%s %s: %s%s", LOG_COLOR[loge], log_level_str[2], tag, Temp_buffer, COLOR_END);
#endif

	plog(Output_buffer);
	free(Temp_buffer);
	free(Output_buffer);
}

/**
 * @fn void LOG_DEBUG(const char*, const char*, ...)
 * @brief
 *
 * @pre
 * @post
 * @param tag
 * @param format
 */
void LOG_DEBUG(const char *tag,  const char *format, ...){
#if CONFIG_LOG_MONITOR_TICK
	uint32_t time = HAL_GetTick();
#endif
	char *Temp_buffer = NULL;
	va_list args;
	va_start(args, format);
	vasprintf(&Temp_buffer, format, args);
	va_end(args);

	char *Output_buffer;
#if CONFIG_LOG_MONITOR_TICK
	asprintf(&Output_buffer, "\r\n%s%s [%lu] %s: %s%s", LOG_COLOR[logd], log_level_str[3], time, tag, Temp_buffer, COLOR_END);
#else
	asprintf(&Output_buffer, "\r\n%s%s %s: %s%s", LOG_COLOR[logd], log_level_str[3], tag, Temp_buffer, COLOR_END);
#endif

	plog(Output_buffer);
	free(Temp_buffer);
	free(Output_buffer);
}

/**
 * @fn void LOG_MEM(const char*, const char*, ...)
 * @brief
 *
 * @pre
 * @post
 * @param tag
 * @param format
 */
void LOG_MEM(const char *tag,  const char *format, ...){
#if CONFIG_LOG_MONITOR_TICK
	uint32_t time = HAL_GetTick();
#endif
	char *Temp_buffer = NULL;
	va_list args;
	va_start(args, format);
	vasprintf(&Temp_buffer, format, args);
	va_end(args);

	char *Output_buffer;
#if CONFIG_LOG_MONITOR_TICK
	asprintf(&Output_buffer, "\r\n%s%s [%lu] %s: %s%s", LOG_COLOR[logm], log_level_str[4], time, tag, Temp_buffer, COLOR_END);
#else
	asprintf(&Output_buffer, "\r\n%s%s %s: %s%s", LOG_COLOR[logm], log_level_str[4], tag, Temp_buffer, COLOR_END);
#endif

	plog(Output_buffer);
	free(Temp_buffer);
	free(Output_buffer);
}

/**
 * @fn void LOG_EVENT(const char*, const char*, ...)
 * @brief
 *
 * @pre
 * @post
 * @param tag
 * @param format
 */
void LOG_EVENT(const char *tag,  const char *format, ...){
#if CONFIG_LOG_MONITOR_TICK
	uint32_t time = HAL_GetTick();
#endif
	char *Temp_buffer = NULL;
	va_list args;
	va_start(args, format);
	vasprintf(&Temp_buffer, format, args);
	va_end(args);

	char *Output_buffer;
#if CONFIG_LOG_MONITOR_TICK
	asprintf(&Output_buffer, "\r\n%s%s [%lu] %s: %s%s", LOG_COLOR[logv], log_level_str[5], time, tag, Temp_buffer, COLOR_END);
#else
	asprintf(&Output_buffer, "\r\n%s%s %s: %s%s", LOG_COLOR[logv], log_level_str[5], tag, Temp_buffer, COLOR_END);
#endif

	plog(Output_buffer);
	free(Temp_buffer);
	free(Output_buffer);
}

/**
 * @fn void LOG_RET(const char*, const char*, ...)
 * @brief
 *
 * @pre
 * @post
 * @param tag
 * @param format
 */
void LOG_RET(const char *tag,  const char *format, ...){
#if CONFIG_LOG_MONITOR_TICK
	uint32_t time = HAL_GetTick();
#endif
	char *Temp_buffer = NULL;
	va_list args;
	va_start(args, format);
	vasprintf(&Temp_buffer, format, args);
	va_end(args);

	char *Output_buffer;
#if CONFIG_LOG_MONITOR_TICK
	asprintf(&Output_buffer, "\r\n%s%s [%lu] %s: %s%s", LOG_COLOR[logr], log_level_str[6], time, tag, Temp_buffer, COLOR_END);
#else
	asprintf(&Output_buffer, "\r\n%s%s %s: %s%s", LOG_COLOR[logr], log_level_str[6], tag, Temp_buffer, COLOR_END);
#endif

	plog(Output_buffer);
	free(Temp_buffer);
	free(Output_buffer);
}

