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

#ifndef _Download_Agent_Plugin_Drm_H
#define _Download_Agent_Plugin_Drm_H

#include "download-agent-type.h"

da_bool_t EDRM_convert(const char *in_file_path, char **out_file_path);
da_ret_t EDRM_wm_get_license(char *rights_url, char **out_content_url);

#endif
