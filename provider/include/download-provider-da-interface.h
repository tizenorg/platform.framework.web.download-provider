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

#ifndef DOWNLOAD_PROVIDER2_DA_INTERFACE_H
#define DOWNLOAD_PROVIDER2_DA_INTERFACE_H

int dp_is_file_exist(const char *file_path);
int dp_init_agent();
void dp_deinit_agent();
dp_error_type dp_start_agent_download(dp_request_slots *request);
dp_error_type dp_resume_agent_download(int req_id);
dp_error_type dp_pause_agent_download(int req_id);
dp_error_type dp_cancel_agent_download(int req_id);
int dp_is_alive_download(int req_id);

#endif
