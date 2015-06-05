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

#ifndef _Download_Agent_Basic_H
#define _Download_Agent_Basic_H

#include "download-agent-dl-info.h"

da_ret_t start_download(da_info_t *da_info);
da_ret_t cancel_download(int dl_id, da_bool_t is_enable_cb);
da_ret_t suspend_download(int dl_id, da_bool_t is_enable_cb);
da_ret_t resume_download(int dl_id);

#endif
