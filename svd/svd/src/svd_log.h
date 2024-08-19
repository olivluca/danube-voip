/**
 * @file svd_log.h
 * Logging macroses.
 * It containes all logging definitions, helps to preserve common behavior.
 */
#ifndef __SVD_LOG_H__
#define __SVD_LOG_H__

/*
 * The logging levels and macros to use are defined as follows:
 *  - SU_DEBUG_0()  fatal errors, panic
 *  - SU_DEBUG_1()  critical errors, minimal progress at subsystem level
 *  - SU_DEBUG_2()  non-critical errors
 *  - SU_DEBUG_3()  warnings, progress messages
 *  - SU_DEBUG_5()  signaling protocol actions (incoming packets, etc.)
 *  - SU_DEBUG_7()  media protocol actions (incoming packets, etc.)
 *  - SU_DEBUG_9()  entering/exiting functions, very verbatim progress
*/

/** @defgroup LOG_MACRO Logging macroses.
 *  Macro-definitions helps to create logs.
 *  @{*/
/** Log with function.*/
#define LOG_FNC_A(str)	"%s(): %s\n",__func__,str
/** Not enough memory template.*/
#define LOG_NOMEM 	" not enough memory"
/** Not enough memory template with user message.*/
#define LOG_NOMEM_A(str) " not enough memory for \"" str "\""
/** File error template with user message (filename).*/
#define LOG_NOFILE_A(str) " could not operate on file \"" str "\""

#if 0
	#define DEBUG_CODE(code) 	do { code }while(0)
#else
	#define DEBUG_CODE(code)
#endif

/** Function start marker.*/
#define DFS												\
	do {												\
		for(g_f_cnt=0; g_f_cnt<g_f_offset; g_f_cnt++)	\
			SU_DEBUG_9(("  " VA_NONE));							\
		SU_DEBUG_9(("vvvv %s() vvvv\n", __func__));		\
		g_f_offset++; 									\
	}while(0);

/** Function end marker.*/
#define DFE												\
	do {												\
		g_f_offset--; 									\
		for(g_f_cnt=0; g_f_cnt<g_f_offset; g_f_cnt++)	\
			SU_DEBUG_9(("  " VA_NONE));							\
		SU_DEBUG_9(("^^^^ %s() ^^^^\n", __func__));		\
	}while(0);

/** Function start/end offset counter.*/
extern unsigned int g_f_cnt;
/** Function start/end offset current value.*/
extern unsigned int g_f_offset;
/** @}*/

#endif /* __SVD_LOG_H__ */

