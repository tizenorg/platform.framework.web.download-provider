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
 * @file		download-agent-plugin-install.c
 * @brief		Platform dependency functions for installation a content to target
 * @author		Jungki,Kwak(jungki.kwak@samsung.com)
 ***/

#include "download-agent-plugin-install.h"
#include "download-agent-debug.h"
#include "download-agent-plugin-conf.h"

#define DA_DEFAULT_INSTALL_PATH_FOR_PHONE "/opt/media/Downloads"
#define DA_DEFAULT_INSTALL_PATH_FOR_MMC "/opt/storage/sdcard/Downloads"

char *PI_get_default_install_dir(void)
{
	da_storage_type_t type;

	if (DA_RESULT_OK != get_storage_type(&type))
		return NULL;

	/* ToDo: need to refactoring later */
	if (type == DA_STORAGE_MMC)
		return DA_DEFAULT_INSTALL_PATH_FOR_MMC;
	else
		return DA_DEFAULT_INSTALL_PATH_FOR_PHONE;
}
