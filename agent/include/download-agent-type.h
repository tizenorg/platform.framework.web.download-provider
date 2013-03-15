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

#ifndef _Download_Agent_Types_H
#define _Download_Agent_Types_H

#include "download-agent-defs.h"

typedef int	da_result_t;
typedef int	da_bool_t;

#define IS_NOT_VALID_ID(x)  (x <= DA_INVALID_ID)

#define DA_MAX_URI_LEN			1024
#define DA_MAX_FULL_PATH_LEN	356	// need configuration
#define DA_MAX_FILE_PATH_LEN		256	// need configuration
#define DA_MAX_STR_LEN			256
#define DA_MAX_MIME_STR_LEN     	256
#define DA_MAX_PROXY_ADDR_LEN	64		// e.g. 100.200.300.400:10000

#endif

