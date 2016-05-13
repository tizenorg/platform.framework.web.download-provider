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

#include <unistd.h>
#include <time.h>
#include <errno.h>

#include "download-provider-log.h"
#include "download-provider-pthread.h"

static char *__print_pthread_error(int code)
{
	switch(code)
	{
		case 0:
			return "NONE";
		case EINVAL:
			return "EINVAL";
		case ENOMEM:
			return "ENOMEM";
		case EBUSY:
			return "EBUSY";
		case EDEADLK:
			return "EDEADLK";
	}
	return "UNKNOWN";
}

int dp_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr)
{
	int ret = pthread_mutex_init(mutex, attr);
	if (0 == ret || EBUSY == ret)
		return 0;
	else
		TRACE_ERROR("error:%d.%s", ret, __print_pthread_error(ret));
	return -1;
}

void dp_mutex_lock(pthread_mutex_t *mutex, const char *func, int line)
{
	int ret = pthread_mutex_lock(mutex);
	if (ret != 0)
		TRACE_ERROR("%s:%d error:%d.%s", func, line, ret,
			__print_pthread_error(ret));
}

int dp_mutex_check_lock(pthread_mutex_t *mutex, const char *func, int line)
{
	int ret = pthread_mutex_lock(mutex);
	if (ret != 0)
		TRACE_ERROR("%s:%d error:%d.%s", func, line, ret,
			__print_pthread_error(ret));
	return ret;
}

int dp_mutex_trylock(pthread_mutex_t *mutex, const char *func, int line)
{
	int ret = pthread_mutex_trylock(mutex);
	if (ret != 0 && ret != EINVAL) {
		TRACE_ERROR("%s:%d error:%d.%s", func, line, ret,
			__print_pthread_error(ret));
	}
	return ret;
}

int dp_mutex_timedlock(pthread_mutex_t *mutex, int sec, const char *func, int line)
{
	struct timespec deltatime;
	deltatime.tv_sec = sec;
	deltatime.tv_nsec = 0;
	int ret = pthread_mutex_timedlock(mutex, &deltatime);
	if (ret != 0) {
		TRACE_ERROR("%s:%d error:%d.%s", func, line, ret,
			__print_pthread_error(ret));
	}
	return ret;
}

void dp_mutex_unlock(pthread_mutex_t *mutex, const char *func, int line)
{
	int ret = pthread_mutex_unlock(mutex);
	if (ret != 0)
		TRACE_ERROR("%s:%d error:%d.%s", func, line, ret,
			__print_pthread_error(ret));
}

void dp_mutex_destroy(pthread_mutex_t *mutex)
{
	int ret = pthread_mutex_destroy(mutex);
	if (ret != 0) {
		TRACE_ERROR("error:%d.%s", ret, __print_pthread_error(ret));
		if(EBUSY == ret) {
			if (pthread_mutex_unlock(mutex) == 0)
				pthread_mutex_destroy(mutex);
		}
	}
}
