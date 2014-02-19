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

#ifndef _Download_Agent_Debug_H
#define _Download_Agent_Debug_H

#include "download-agent-type.h"

#define DA_DEBUG_ENV_KEY "DOWNLOAD_AGENT_DEBUG"
#define DA_DEBUG_CONFIG_FILE_PATH "/tmp/.download_agent.conf"

#define IS_LOG_ON(channel) (DALogBitMap & (0x1<<(channel)))

typedef enum {
	Soup,
	HTTPManager,
	FileManager,
	DRMManager,
	DownloadManager,
	ClientNoti,
	HTTPMessageHandler,
	Encoding,
	QueueManager,
	Parsing,
	Thread,
	Default,
	DA_LOG_CHANNEL_MAX
} da_log_channel;

extern int DALogBitMap;

da_result_t init_log_mgr(void);

#ifdef NODEBUG
	#define DA_LOG(channel, format, ...) ((void)0)
	#define DA_LOG_CRITICAL(channel, format, ...) ((void)0)
	#define DA_LOG_VERBOSE(channel, format, ...) ((void)0)
	#define DA_LOG_ERR(channel, format, ...) ((void)0)
	#define DA_LOG_FUNC_LOGD(channel, ...) ((void)0)
	#define DA_LOG_FUNC_LOGV(channel, ...) ((void)0)
	#define DA_SECURE_LOGD(format, ...) ((void)0)
	#define DA_SECURE_LOGI(format, ...) ((void)0)
	#define DA_SECURE_LOGE(format, ...) ((void)0)

#else /* NODEBUG */
#include <stdio.h>
#include <stdarg.h>
#include <pthread.h>

#ifdef DA_DEBUG_USING_DLOG
	#include <dlog.h>
	#ifdef LOG_TAG
	#undef LOG_TAG
	#endif /*  LOG_TAG */
	#define LOG_TAG "DOWNLOAD_AGENT"

	#define DA_LOG(channel, format, ...) LOGI_IF(IS_LOG_ON(channel), format, ##__VA_ARGS__)
	#define DA_LOG_DEBUG(channel, format, ...) LOGD_IF(IS_LOG_ON(channel), format, ##__VA_ARGS__)
	#define DA_LOG_CRITICAL(channel, format, ...) LOGI_IF(IS_LOG_ON(channel), format, ##__VA_ARGS__)
	#define DA_LOG_VERBOSE(channel, format, ...) ((void)0)//LOGD_IF(IS_LOG_ON(channel), format, ##__VA_ARGS__)
	#define DA_LOG_ERR(channel, format, ...) LOGE_IF(IS_LOG_ON(channel), "ERR! "format, ##__VA_ARGS__)
	#define DA_LOG_FUNC_LOGD(channel, ...) LOGD_IF(IS_LOG_ON(channel), "starting...")
	#define DA_LOG_FUNC_LOGV(channel, ...) ((void)0)//LOGD_IF(IS_LOG_ON(channel), "starting...")
	#define DA_SECURE_LOGD(format, ...) SECURE_LOGD(format, ##__VA_ARGS__)
	#define DA_SECURE_LOGI(format, ...) SECURE_LOGI(format, ##__VA_ARGS__)
	#define DA_SECURE_LOGE(format, ...) SECURE_LOGE(format, ##__VA_ARGS__)
#else /* DA_DEBUG_USING_DLOG */
	#include <unistd.h>
	#include <syscall.h>

	#define DA_LOG(channel, format, ...) do {\
		IS_LOG_ON(channel) \
				? fprintf(stderr, "[DA][%u][%s(): %d] "format"\n",(unsigned int)syscall(__NR_gettid), __FUNCTION__,__LINE__, ##__VA_ARGS__) \
				: ((void)0);\
	}while(0)
	#define DA_LOG_ERR(channel, format, ...) do {\
		IS_LOG_ON(channel) \
				? fprintf(stderr, "[DA][%u][ERR][%s(): %d]\n",(unsigned int)syscall(__NR_gettid), __FUNCTION__,__LINE__, ##__VA_ARGS__) \
				: ((void)0); \
	}while(0)
	#define DA_LOG_FUNC_LOGD(channel, ...) do {\
		IS_LOG_ON(channel) \
				? fprintf(stderr, "[DA][%u][%s(): %d] starting\n",(unsigned int)syscall(__NR_gettid), __FUNCTION__,__LINE__) \
				: ((void)0); \
	}while(0)
	#define DA_LOG_CRITICAL DA_LOG
	#define DA_LOG_VERBOSE DA_LOG
	#define DA_LOG_FUNC_LOGV ((void)0)
	#define DA_SECURE_LOGD(format, ...) ((void)0)
	#define DA_SECURE_LOGI(format, ...) ((void)0)
	#define DA_SECURE_LOGE(format, ...) ((void)0)
#endif /* DA_DEBUG_USING_DLOG */
#endif /* NDEBUG */
#endif /* _Download_Agent_Debug_H */
