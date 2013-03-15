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

#ifndef DOWNLOAD_PROVIDER2_NOTIFICATION_H
#define DOWNLOAD_PROVIDER2_NOTIFICATION_H

#include "download-provider-config.h"

int dp_set_downloadinginfo_notification(int id, char *packagename);
int dp_set_downloadedinfo_notification(int priv_id, int id, char *packagename, dp_state_type state);
void dp_update_downloadinginfo_notification(int priv_id, double received_size, double file_size);
void dp_clear_downloadinginfo_notification(void);

#endif
