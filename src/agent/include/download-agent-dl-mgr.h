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

#ifndef _Download_Agent_Dl_Mgr_H
#define _Download_Agent_Dl_Mgr_H

#include "download-agent-type.h"
#include "download-agent-dl-info-util.h"

da_result_t  cancel_download(int dl_id);
da_result_t  suspend_download(int dl_id, da_bool_t is_enable_cb);
da_result_t  resume_download (int dl_id);

da_result_t  requesting_download(stage_info *stage);
da_result_t  handle_after_download(stage_info *stage);
da_result_t  send_user_noti_and_finish_download_flow(
		int slot_id, char *installed_path, char *etag);

da_bool_t is_valid_download_id(int dl_id);
#endif
