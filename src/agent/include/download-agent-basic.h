/*
 * Download Agent
 *
 * Copyright (c) 2000 - 2012 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact: Jungki Kwak <jungki.kwak@samsung.com>, Keunsoon Lee <keunsoon.lee@samsung.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * @file		download-agent-basic.h
 * @brief
 * @author		Keunsoon Lee(keunsoon.lee@samsung.com)
 ***/

#ifndef _Download_Agent_Basic_H
#define _Download_Agent_Basic_H

#include <string.h>

#include "download-agent-type.h"
#include "download-agent-interface.h"
#include "download-agent-dl-mgr.h"

typedef struct _extension_data_t {
	const char **request_header;
	const int *request_header_count;
	const char *install_path;
	const char *file_name;
	void *user_data;
} extension_data_t;

da_result_t start_download(const char* url, da_handle_t *dl_req_id);
da_result_t start_download_with_extension(const char *url , da_handle_t *dl_req_id, extension_data_t *extension_data);

#endif
