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

#ifndef _Download_Agent_Http_Mgr_H
#define _Download_Agent_Http_Mgr_H

#include <string.h>

#include "download-agent-type.h"
#include "download-agent-dl-mgr.h"

#define	DA_MAX_SESSION_INFO				DA_MAX_DOWNLOAD_ID

da_ret_t request_http_download(da_info_t *da_info);
da_ret_t  request_to_cancel_http_download(da_info_t *da_info);
da_ret_t  request_to_abort_http_download(da_info_t *da_info);
da_ret_t  request_to_suspend_http_download(da_info_t *da_info);
da_ret_t  request_to_resume_http_download(da_info_t *da_info);
da_bool_t is_stopped_state(da_info_t *da_info);

#endif
