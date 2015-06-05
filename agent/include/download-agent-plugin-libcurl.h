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

#ifndef _Download_Agent_Plugin_Libcurl_H
#define _Download_Agent_Plugin_Libcrul_H

#include "download-agent-type.h"
#include "download-agent-dl-info.h"

#define MAX_SESSION_COUNT	DA_MAX_DOWNLOAD_REQ_AT_ONCE
#define MAX_TIMEOUT			DA_MAX_TIME_OUT

da_ret_t PI_http_start(da_info_t *da_info);
da_ret_t PI_http_disconnect(http_info_t *info);
da_ret_t PI_http_cancel(http_info_t *info);
da_ret_t PI_http_pause(http_info_t *info);
da_ret_t PI_http_unpause(http_info_t *info);
#ifdef _RAF_SUPPORT
da_ret_t PI_http_set_file_name_to_curl(http_msg_t *http_msg, char *file_path);
#endif

#endif
