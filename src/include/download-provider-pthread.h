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

#ifndef DOWNLOAD_PROVIDER2_PTHREAD_H
#define DOWNLOAD_PROVIDER2_PTHREAD_H

#ifdef __cplusplus
extern "C" {
#endif

#include <unistd.h>
#include <pthread.h>
#include <errno.h>

// download-provider use default style mutex.

#define CLIENT_MUTEX_LOCK(mutex_add) {\
	int ret = 0;\
	ret = pthread_mutex_lock(mutex_add);\
	if (EINVAL == ret) {\
		TRACE_STRERROR("ERR:pthread_mutex_lock FAIL with EINVAL.");\
	} else if (EDEADLK == ret) {\
		TRACE_STRERROR("ERR:pthread_mutex_lock FAIL with EDEADLK.");\
	} else if (ret != 0) {\
		TRACE_STRERROR("ERR:pthread_mutex_lock FAIL with %d.", ret);\
	} \
}

#define CLIENT_MUTEX_UNLOCK(mutex_add) {\
	int ret = 0;\
	ret = pthread_mutex_unlock(mutex_add);\
	if (EINVAL == ret) {\
		TRACE_STRERROR("ERR:pthread_mutex_unlock FAIL with EINVAL.");\
	} else if (EDEADLK == ret) {\
		TRACE_STRERROR("ERR:pthread_mutex_unlock FAIL with EDEADLK.");\
	} else if (ret != 0) {\
		TRACE_STRERROR("ERR:pthread_mutex_unlock FAIL with %d.", ret);\
	} \
}

#define CLIENT_MUTEX_DESTROY(mutex_add) { \
	int ret = 0; \
	ret = pthread_mutex_destroy(mutex_add); \
	if(EINVAL == ret) {\
		TRACE_STRERROR("ERR:pthread_mutex_destroy FAIL with EINVAL."); \
	} else if(ENOMEM == ret) {\
		TRACE_STRERROR("ERR:pthread_mutex_destroy FAIL with ENOMEM."); \
	} else if(EBUSY == ret) {\
		TRACE_STRERROR("ERR:pthread_mutex_destroy FAIL with EBUSY."); \
		if (pthread_mutex_unlock(mutex_add) == 0) \
			pthread_mutex_destroy(mutex_add); \
	} else if (ret != 0) {\
		TRACE_STRERROR("ERR:pthread_mutex_destroy FAIL with %d.", ret); \
	} \
}

#define CLIENT_MUTEX_INIT(mutex_add, attr) { \
	int ret = 0; \
	unsigned retry = 3; \
	while (retry > 0) { \
		ret = pthread_mutex_init(mutex_add, attr); \
		if (0 == ret) { \
			break; \
		} else if(EINVAL == ret) { \
			TRACE_STRERROR("ERR:pthread_mutex_init FAIL with EINVAL."); \
		} else if(ENOMEM == ret) { \
			TRACE_STRERROR("ERR:pthread_mutex_init FAIL with ENOMEM."); \
			usleep(1000); \
		} else if(EBUSY == ret) {\
			TRACE_STRERROR("ERR:pthread_mutex_destroy FAIL with EBUSY."); \
			if (pthread_mutex_unlock(mutex_add) == 0) \
				pthread_mutex_destroy(mutex_add); \
		} else if (ret != 0) { \
			TRACE_STRERROR("ERR:pthread_mutex_init FAIL with %d.", ret); \
		} \
		retry--; \
	} \
}

#ifdef __cplusplus
}
#endif
#endif
