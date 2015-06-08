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

#ifndef DOWNLOAD_PROVIDER_PTHREAD_H
#define DOWNLOAD_PROVIDER_PTHREAD_H

#ifdef __cplusplus
extern "C" {
#endif

#include <pthread.h>

// download-provider use default style mutex.
int dp_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr);
void dp_mutex_lock(pthread_mutex_t *mutex, const char *func, int line);
int dp_mutex_check_lock(pthread_mutex_t *mutex, const char *func, int line);
int dp_mutex_trylock(pthread_mutex_t *mutex, const char *func, int line);
int dp_mutex_timedlock(pthread_mutex_t *mutex, int sec, const char *func, int line);
void dp_mutex_unlock(pthread_mutex_t *mutex, const char *func, int line);
void dp_mutex_destroy(pthread_mutex_t *mutex);

#define CLIENT_MUTEX_LOCK(mutex) dp_mutex_lock(mutex, __FUNCTION__, __LINE__)
#define CLIENT_MUTEX_CHECKLOCK(mutex) dp_mutex_check_lock(mutex, __FUNCTION__, __LINE__)
#define CLIENT_MUTEX_TRYLOCK(mutex) dp_mutex_trylock(mutex, __FUNCTION__, __LINE__)
#define CLIENT_MUTEX_TIMEDLOCK(mutex, sec) dp_mutex_timedlock(mutex, sec, __FUNCTION__, __LINE__)
#define CLIENT_MUTEX_UNLOCK(mutex) dp_mutex_unlock(mutex, __FUNCTION__, __LINE__)

#ifdef __cplusplus
}
#endif
#endif
