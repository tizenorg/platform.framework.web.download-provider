/*
 * Copyright (c) 2012 Samsung Electronics Co., Ltd All Rights Reserved
 *
 * Licensed under the Apache License, Version 2.0 (the License);
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an AS IS BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _DOWNLOAD_AGENT_DEBUG_H
#define _DOWNLOAD_AGENT_DEBUG_H

#include "download-agent-type.h"

#include <stdio.h>
#include <stdarg.h>
#include <pthread.h>

// ansi color
#define COLOR_RED 		"\033[0;31m"
#define COLOR_GREEN 	"\033[0;32m"
#define COLOR_BROWN 	"\033[0;33m"
#define COLOR_LIGHTBLUE "\033[0;37m"
#define COLOR_END		"\033[0;m"

#ifdef _ENABLE_DLOG
#include <unistd.h>
#include <syscall.h>
#include <dlog.h>

	#ifdef LOG_TAG
	#undef LOG_TAG
	#endif /*  LOG_TAG */

	#define LOG_TAG "DP_DA"
	#define DA_LOGV(format, ...) ((void)0)//LOGD("[%d]:"format, syscall(__NR_gettid), ##__VA_ARGS__)
	#define DA_LOGD(format, ...) LOGD(COLOR_LIGHTBLUE "[%d]:"format COLOR_END, syscall(__NR_gettid), ##__VA_ARGS__)
	#define DA_LOGI(format, ...) LOGI(COLOR_BROWN "[%d]:"format COLOR_END, syscall(__NR_gettid), ##__VA_ARGS__)
	#define DA_LOGE(format, ...) LOGE(COLOR_RED "[%d]:"format COLOR_END, syscall(__NR_gettid), ##__VA_ARGS__)
	#define DA_SECURE_LOGD(format, ...) SECURE_LOGD(COLOR_GREEN format COLOR_END, ##__VA_ARGS__)
	#define DA_SECURE_LOGI(format, ...) SECURE_LOGI(COLOR_GREEN format COLOR_END, ##__VA_ARGS__)
	#define DA_SECURE_LOGE(format, ...) SECURE_LOGE(COLOR_GREEN format COLOR_END, ##__VA_ARGS__)
#else

#include <unistd.h>
#include <syscall.h>

	#define DA_LOGD(format, ...) do {\
				fprintf(stderr, "[DA][%d][%s():%d] "format"\n",syscall(__NR_gettid), __FUNCTION__,__LINE__, ##__VA_ARGS__);\
	}while(0)
	#define DA_LOGE(format, ...) do {\
				fprintf(stderr, "[DA][%d][ERR][%s():%d]\n",syscall(__NR_gettid), __FUNCTION__,__LINE__, ##__VA_ARGS__);\
	}while(0)
	#define DA_LOGV DA_LOGD
	#define DA_LOGI DA_LOGD
	#define DA_SECURE_LOGD(format, ...) ((void)0)
	#define DA_SECURE_LOGI(format, ...) ((void)0)
	#define DA_SECURE_LOGE(format, ...) ((void)0)
#endif /* _ENABLE_DLOG */

#endif /* DOWNLOAD_AGENT_DEBUG_H */
