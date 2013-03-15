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

#include <string.h>

#include "download-agent-type.h"
#include "download-agent-interface.h"
#include "download-agent-dl-mgr.h"

da_result_t start_download(const char *url, int *dl_id);
da_result_t start_download_with_extension(const char *url , int *dl_id, extension_data_t *extension_data);

#endif
