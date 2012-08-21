#ifndef DOWNLOAD_PROVIDER_PTHREAD_H
#define DOWNLOAD_PROVIDER_PTHREAD_H

#ifdef __cplusplus
extern "C" {
#endif

#include <pthread.h>
#include <errno.h>

#define CLIENT_MUTEX_INIT(mutex_add, attr) { \
	int ret = 0; \
	do { \
		ret = pthread_mutex_init(mutex_add, attr); \
		if (0 == ret) { \
			break; \
		} else if(EINVAL == ret) { \
			TRACE_DEBUG_MSG("ERR:pthread_mutex_init FAIL with EINVAL."); \
			break; \
		} else if(ENOMEM == ret) { \
			TRACE_DEBUG_MSG("ERR:pthread_mutex_init FAIL with ENOMEM."); \
			break; \
		} else { \
			TRACE_DEBUG_MSG("ERR:pthread_mutex_init FAIL with %d.", ret); \
			break; \
		} \
	}while(1); \
}

#define CLIENT_MUTEX_LOCK(mutex_add) {\
	int ret = 0;\
	do {\
		ret = pthread_mutex_lock(mutex_add);\
		if (0 == ret) {\
			break;\
		} else if (EINVAL == ret) {\
			TRACE_DEBUG_MSG("ERR:pthread_mutex_lock FAIL with EINVAL.");\
			break;\
		} else if (EDEADLK == ret) {\
			TRACE_DEBUG_MSG("ERR:pthread_mutex_lock FAIL with EDEADLK.");\
			break;\
		} else {\
			TRACE_DEBUG_MSG("ERR:pthread_mutex_lock FAIL with %d.", ret);\
			break;\
		} \
	} while(1);\
}

#define CLIENT_MUTEX_UNLOCK(mutex_add) {\
	int ret = 0;\
	do {\
		ret = pthread_mutex_unlock(mutex_add);\
		if (0 == ret) {\
			break;\
		} else if (EINVAL == ret) {\
			TRACE_DEBUG_MSG("ERR:pthread_mutex_lock FAIL with EINVAL.");\
			break;\
		} else if (EDEADLK == ret) {\
			TRACE_DEBUG_MSG("ERR:pthread_mutex_lock FAIL with EDEADLK.");\
			break;\
		} else {\
			TRACE_DEBUG_MSG("ERR:pthread_mutex_lock FAIL with %d.", ret);\
			break;\
		} \
	} while(1);\
}

#define CLIENT_MUTEX_DESTROY(mutex_add) { \
	int ret = 0; \
	do { \
		ret = pthread_mutex_destroy(mutex_add); \
		if (0 == ret) { \
			break; \
		} else if(EINVAL == ret) { \
			TRACE_DEBUG_MSG("ERR:pthread_mutex_destroy FAIL with EINVAL."); \
			break; \
		} else if(ENOMEM == ret) { \
			TRACE_DEBUG_MSG("ERR:pthread_mutex_destroy FAIL with ENOMEM."); \
			break; \
		} else { \
			TRACE_DEBUG_MSG("ERR:pthread_mutex_destroy FAIL with %d.", ret); \
			break; \
		} \
	}while(1); \
}

#ifdef __cplusplus
}
#endif
#endif
