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

#ifndef _DOWNLOAD_AGENT_PTHREAD_H
#define _DOWNLOAD_AGENT_PTHREAD_H

#include <pthread.h>
#include <errno.h>
#include <time.h>

#include "download-agent-debug.h"

#define DA_MUTEX_INIT(mutex_add, attr) {\
	int ret = 0;\
	do {\
		ret = pthread_mutex_init(mutex_add, attr);\
		if (0 == ret){\
			break;\
		}\
		else if (EINVAL == ret){\
			DA_LOGE("pthread_mutex_init FAIL with EINVAL.");\
			break;\
		}\
		else if (ENOMEM == ret){\
			DA_LOGE("pthread_mutex_init FAIL with ENOMEM.");\
			break;\
		}\
		else{\
			DA_LOGE("pthread_mutex_init FAIL with %d.", ret);\
			break;\
		}\
	} while(1);\
}

#define DA_COND_INIT(cond_add, attr) do {\
	if (0 != pthread_cond_init(cond_add, attr)){\
		DA_LOGE("pthread_cond_init FAIL");\
	}\
} while(0)

#define DA_MUTEX_LOCK(mutex_add) {\
	int ret = 0;\
	do {\
		ret = pthread_mutex_lock(mutex_add);\
		if (0 == ret){\
			break;\
		}\
		else if (EINVAL == ret){\
			DA_LOGE("pthread_mutex_lock FAIL with EINVAL.");\
			break;\
		}\
		else if (EDEADLK == ret){\
			DA_LOGE("pthread_mutex_lock FAIL with EDEADLK.");\
			break;\
		}\
		else{\
			DA_LOGE("pthread_mutex_lock FAIL with %d.", ret);\
			break;\
		}\
	} while(1);\
}

#define DA_MUTEX_UNLOCK(mutex_add) {\
	int ret = 0;\
	do {\
		ret = pthread_mutex_unlock(mutex_add);\
		if (0 == ret){\
			break;\
		}\
		else if (EINVAL == ret) {\
			DA_LOGE("pthread_mutex_unlock FAIL with EINVAL.");\
			break;\
		}\
		else if (EPERM == ret) {\
			DA_LOGE("pthread_mutex_unlock FAIL with EPERM.");\
			break;\
		}\
		else {\
			DA_LOGE("pthread_mutex_unlock FAIL with %d.", ret);\
			break;\
		}\
	} while(1);\
}

#define DA_COND_SIGNAL(cond_add) do {\
		if (0 != pthread_cond_signal(cond_add)) {\
			DA_LOGE("pthread_cond_signal FAIL");\
		}\
	} while(0)

#define DA_COND_WAIT(cond_add, mutex_add) do {\
		if (0 != pthread_cond_wait(cond_add, mutex_add)){\
			DA_LOGE("pthread_cond_wait FAIL");\
		}\
	} while(0)

#define DA_COND_TIMED_WAIT(cond_add, mutex_add, time) do {\
		if (0 != pthread_cond_timedwait(cond_add, mutex_add, time)){\
			DA_LOGE("pthread_cond_wait FAIL");\
		}\
	} while(0)


#define DA_COND_DESTROY(cond_add)	do {\
		if (0 != pthread_cond_destroy(cond_add)){\
			DA_LOGE("pthread_cond_destroy FAIL");\
		}\
	} while(0)

#define DA_MUTEX_DESTROY(mutex_add) {\
	int ret = 0;\
	do {\
		ret = pthread_mutex_destroy(mutex_add);\
		if (0 == ret){\
			break;\
		}\
		else if (EINVAL == ret){\
			DA_LOGE("pthread_mutex_destroy FAIL with EINVAL.");\
			break;\
		}\
		else if (EBUSY == ret){\
			DA_LOGE("pthread_mutex_destroy FAIL with EBUSY.");\
			break;\
		}\
		else {\
			DA_LOGE("pthread_mutex_destroy FAIL with %d.", ret);\
			break;\
		}\
	} while(1);\
}
#endif
