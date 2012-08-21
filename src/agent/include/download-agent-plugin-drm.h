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
 * @file		download-agent-plugin-drm.h
 * @brief		Including plugin functions for emerald DRM from SISO
 * @author		Keunsoon Lee(keunsoon.lee@samsung.com)
 ***/


#ifndef _Download_Agent_Plugin_Drm_H
#define _Download_Agent_Plugin_Drm_H

#include "download-agent-type.h"

da_bool_t EDRM_check_dcf_file(const char *file_path);
da_bool_t EDRM_has_vaild_ro(const char *file_path);
da_bool_t EDRM_open_convert (const char *file_path, void **fd);
da_bool_t EDRM_write_convert (void *fd, unsigned char *buffer, int buffer_size);
da_bool_t EDRM_close_convert (void **fd);
da_result_t EDRM_wm_get_license (char *rights_url, char **out_content_url);


#endif
