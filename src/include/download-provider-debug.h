/*
 * Copyright (c) 2000 - 2012 Samsung Electronics Co., Ltd All Rights Reserved 
 *
 * This file is part of Download Provider
 * Written by Jungki Kwak <jungki.kwak@samsung.com>
 *
 * PROPRIETARY/CONFIDENTIAL
 *
 * This software is the confidential and proprietary information of SAMSUNG ELECTRONICS
 * ("Confidential Information"). You shall not disclose such Confidential Information
 * and shall use it only in accordance with the terms of the license agreement
 * you entered into with SAMSUNG ELECTRONICS.
 *
 * SAMSUNG make no representations or warranties about the suitability of the software,
 * either express or implied, including but not limited to the implied warranties of merchantability,
 * fitness for a particular purpose, or non-infringement.
 *
 * SAMSUNG shall not be liable for any damages suffered by licensee as a result of using,
 * modifying or distributing this software or its derivatives.
 */
/**
 * @file 	download-provider-debug.h
 * @author	Jungki Kwak (jungki.kwak@samsung.com)
 * @brief	debug function
 */

#ifndef DOWNLOAD_PROVIDER_DEBUG_H
#define DOWNLOAD_PROVIDER_DEBUG_H

#define _USE_DLOG 1

#ifdef _USE_DLOG
#include <dlog.h>

#ifdef LOG_TAG
#undef LOG_TAG
#endif

#define LOG_TAG "DownloadProvider"

#define DP_LOG(format, args...) LOGI("[%s] "format, __func__, ##args)
#define DP_LOGD(format, args...) LOGD("[%s] "format, __func__, ##args)
#define DP_LOG_START(msg) LOGI("<<= [%s] Start =>>\n",msg)
#define DP_LOG_FUNC() LOGI("<<= [%s]=>>\n",__func__)
#define DP_LOGD_FUNC() LOGD("<<= [%s]=>>\n",__func__)
#define DP_LOG_END(msg) LOGI("<<= [%s] End [%d] =>>\n",msg)
#define DP_LOGE(format, args...) LOGE("[%s][ERR] "format, __func__, ##args)
#define DP_LOG_TEST(format, args...) LOGI("####TEST####[%s] "format, __func__, ##args)

#else

#include <stdio.h>
#include <pthread.h>

#define DP_LOG(args...) do {\
		printf("[DP:%s][LN:%d][%lu]",__func__,__LINE__,pthread_self());\
		printf(args);printf("\n"); }while(0)
#define DP_LOGD(args...) do {\
		printf("[DP_D:%s][LN:%d][%lu]",__func__,__LINE__,pthread_self());\
		printf(args);printf("\n");}while(0)
#define DP_LOGE(args...) do {\
		printf("[DP_ERR:%s][LN:%d][%lu]",__func__,__LINE__,pthread_self());\
		printf(args);printf("\n");}while(0)
#define DP_LOG_FUNC() do {\
		printf("<<==[DP:%s][LN:%d][%lu] ==>> \n",__func__,__LINE__,pthread_self());\
		}while(0)
#define DP_LOGD_FUNC() do {\
		printf("<<==[DP_D:%s][LN:%d][%lu] ==>> \n",__func__,__LINE__,pthread_self());\
		}while(0)
#define DP_LOG_START(msg) do {\
		printf("<<==[DP:%s][LN:%d][%lu] Start ==>> \n",\
		__FUNCTION__,__LINE__,pthread_self());\
		}while(0)
#define DP_LOG_END(msg) do {\
		printf("<<==[DP:%s][LN:%d][%lu] End  ==>> \n",\
		__FUNCTION__,__LINE__,pthread_self());\
		}while(0)
#endif /*_USE_DLOG*/

#endif /* DOWNLOAD_PROVIDER_DEBUG_H */
