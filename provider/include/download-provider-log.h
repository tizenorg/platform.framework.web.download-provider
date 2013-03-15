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

#ifndef DOWNLOAD_PROVIDER2_LOG_H
#define DOWNLOAD_PROVIDER2_LOG_H

#include <string.h>
#include <errno.h>

#define DEBUG_MSG
//#define DEBUG_PRINTF

#ifdef DEBUG_MSG
#ifdef DEBUG_PRINTF
#include <stdio.h>
#define TRACE_ERROR(format, ARG...)  \
{ \
fprintf(stderr,"[PROVIDER][%s:%d] "format"\n", __FUNCTION__, __LINE__, ##ARG); \
}
#define TRACE_STRERROR(format, ARG...)  \
{ \
fprintf(stderr,"[PROVIDER][%s:%d] "format" [%s]\n", __FUNCTION__, __LINE__, ##ARG, strerror(errno)); \
}
#define TRACE_INFO(format, ARG...)  \
{ \
fprintf(stderr,"[PROVIDER][%s:%d] "format"\n", __FUNCTION__, __LINE__, ##ARG); \
}
#else
#include <dlog.h>
#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "DOWNLOAD_PROVIDER"
#define TRACE_ERROR(format, ARG...)  \
{ \
LOGE(format, ##ARG); \
}
#define TRACE_STRERROR(format, ARG...)  \
{ \
LOGE(format" [%s]", ##ARG, strerror(errno)); \
}
#define TRACE_INFO(format, ARG...)  \
{ \
LOGI(format, ##ARG); \
}
#endif
#else
#define TRACE_DEBUG_MSG(format, ARG...) ;
#endif
#endif
