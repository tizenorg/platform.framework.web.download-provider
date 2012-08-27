#ifndef DOWNLOAD_PROVIDER_LOG_H
#define DOWNLOAD_PROVIDER_LOG_H
#define DEBUG_MSG

#ifdef DEBUG_MSG
#include <dlog.h>
#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "download-provider"
#define TRACE_DEBUG_MSG(format, ARG...)  \
{ \
LOGE(format"[%s][%d]", ##ARG, __FUNCTION__, __LINE__); \
}
#define TRACE_DEBUG_INFO_MSG(format, ARG...)  \
{ \
LOGI(format"[%s][%d]", ##ARG, __FUNCTION__, __LINE__); \
}
#else
#define TRACE_DEBUG_MSG(format, ARG...) ;
#endif
#endif
