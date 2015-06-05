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

#ifdef SUPPORT_LOG_MESSAGE
#include <errno.h>
#include <dlog.h>
#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG DOWNLOAD_PROVIDER_LOG_TAG
#if defined(LOGD) && defined(TIZEN_DEBUG_ENABLE)
#define TRACE_DEBUG(format, ARG...) LOGD(format, ##ARG)
#else
#define TRACE_DEBUG(...) do { } while(0)
#endif
#define TRACE_ERROR(format, ARG...) LOGE(format, ##ARG)
#define TRACE_STRERROR(format, ARG...) LOGE(format" [%s]", ##ARG, strerror(errno))
#define TRACE_INFO(format, ARG...) LOGI(format, ##ARG)
#define TRACE_WARN(format, ARG...) LOGW(format, ##ARG)

#if defined(SECURE_LOGD) && defined(TIZEN_DEBUG_ENABLE)
#define TRACE_SECURE_DEBUG(format, ARG...) SECURE_LOGD(format, ##ARG)
#else
#define TRACE_SECURE_DEBUG(...) do { } while(0)
#endif
#if defined(SECURE_LOGI) && defined(TIZEN_DEBUG_ENABLE)
#define TRACE_SECURE_INFO(format, ARG...) SECURE_LOGI(format, ##ARG)
#else
#define TRACE_SECURE_INFO(...) do { } while(0)
#endif
#if defined(SECURE_LOGE) && defined(TIZEN_DEBUG_ENABLE)
#define TRACE_SECURE_ERROR(format, ARG...) SECURE_LOGE(format, ##ARG)
#else
#define TRACE_SECURE_ERROR(...) do { } while(0)
#endif

#else
#define TRACE_DEBUG(...) do { } while(0)
#define TRACE_ERROR(...) do { } while(0)
#define TRACE_STRERROR(...) do { } while(0)
#define TRACE_INFO(...) do { } while(0)
#define TRACE_WARN(...) do { } while(0)
#define TRACE_SECURE_DEBUG(...) do { } while(0)
#define TRACE_SECURE_INFO(...) do { } while(0)
#define TRACE_SECURE_ERROR(...) do { } while(0)
#endif

#endif
