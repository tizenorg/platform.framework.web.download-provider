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

#ifndef _DOWNLOAD_AGENT_TYPE_H
#define _DOWNLOAD_AGENT_TYPE_H

#include "download-agent-defs.h"

typedef int	da_ret_t;
typedef int	da_bool_t;
typedef unsigned long long da_size_t;

#define IS_NOT_VALID_ID(x)  (x <= DA_INVALID_ID)

#define DA_MAX_URI_LEN			1024
#define DA_MAX_FULL_PATH_LEN	356	// need configuration
#define DA_MAX_FILE_PATH_LEN		256	// need configuration
#define DA_MAX_STR_LEN			256
#define DA_MAX_MIME_STR_LEN     	256
#define DA_MAX_PROXY_ADDR_LEN	64		// e.g. 100.200.300.400:10000

#define SCHEME_HTTP		"http://"

#define DA_DEFAULT_INSTALL_PATH_FOR_PHONE "/opt/usr/media/Downloads"

#define DA_MAX_ID	DA_MAX_DOWNLOAD_REQ_AT_ONCE

#define SAVE_FILE_BUFFERING_SIZE_50KB (50*1024)

#define NULL_CHECK(DATA) {\
	if (!DATA) {\
		DA_LOGE("NULL CHECK!:%s",(#DATA));\
		return;\
	}\
}

#define NULL_CHECK_RET(DATA) {\
	if (!DATA) {\
		DA_LOGE("NULL CHECK!:%s",(#DATA));\
		return DA_ERR_INVALID_ARGUMENT;\
	}\
}

#define NULL_CHECK_GOTO(DATA) {\
	if (!DATA) {\
		DA_LOGE("NULL CHECK!:%s",(#DATA));\
		ret = DA_ERR_INVALID_ARGUMENT;\
		goto ERR;\
	}\
}

#define NULL_CHECK_RET_OPT(DATA, RET_DATA) {\
	if (!DATA) {\
		DA_LOGE("NULL CHECK!:%s",(#DATA));\
		return RET_DATA;\
	}\
}

#endif

