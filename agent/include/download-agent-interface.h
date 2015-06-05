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

#ifndef _DOWNLOAD_AGENT_INTERFACE_H
#define _DOWNLOAD_AGENT_INTERFACE_H

#ifndef EXPORT_API
#define EXPORT_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C"
{
#endif

#include "download-agent-defs.h"
#include <stdarg.h>

typedef struct {
	int download_id;
	char *file_type;
	unsigned long long file_size;
	char *tmp_saved_path;
	char *content_name;
	char *etag;
} download_info_t;

typedef struct {
	int download_id;
	char *saved_path;
	char *etag;
	int err;
	int http_status;
} finished_info_t;

typedef struct {
	const char **request_header;
	int request_header_count;
	const char *install_path;
	const char *file_name;
	const char *temp_file_path;
	const char *etag;
	const char *pkg_name;
	int network_bonding;
	void *user_req_data;
	void *user_client_data;
} req_data_t;

typedef void (*da_paused_cb) (int download_id,
		void *user_param1, void *user_param2);
typedef void (*da_progress_cb) (int download_id,
		unsigned long long received_size,
		void *user_param1, void *user_param2);
typedef void (*da_started_cb) (download_info_t *download_info,
		void *user_param1, void *user_param2);
typedef void (*da_finished_cb) (finished_info_t *finished_info,
		void *user_param1, void *user_param2);

typedef struct {
	da_started_cb download_info_cb;
	da_progress_cb progress_cb;
	da_finished_cb finished_cb;
	da_paused_cb paused_cb;
} da_cb_t;

EXPORT_API int da_init();
EXPORT_API int da_deinit();

EXPORT_API int da_start_download(const char *url, req_data_t *ext_data,
		da_cb_t *da_cb_data,	int *download_id);
EXPORT_API int da_cancel_download(int download_id);
EXPORT_API int da_cancel_download_without_update(int download_id);
EXPORT_API int da_suspend_download(int download_id);
EXPORT_API int da_suspend_download_without_update(int download_id);
EXPORT_API int da_resume_download(int download_id);
EXPORT_API int da_is_valid_download_id(int download_id);

#ifdef __cplusplus
}
#endif

#endif //_DOWNLOAD_AGENT_INTERFACE_H


