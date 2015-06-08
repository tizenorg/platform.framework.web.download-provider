/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd All Rights Reserved
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

#ifndef DOWNLOAD_PROVIDER_CONFIG_H
#define DOWNLOAD_PROVIDER_CONFIG_H

#include <download-provider.h>

// client-manager
#define DP_MAX_CLIENTS 32 // the maximun number of slots
#define DP_CARE_CLIENT_MANAGER_INTERVAL 120
#define DP_CARE_CLIENT_REQUEST_INTERVAL 120
#define DP_CARE_CLIENT_CLEAR_INTERVAL 200

// database
#define DP_CARE_CLIENT_INFO_PERIOD 48 // hour

// each client thread
#define DP_MAX_REQUEST MAX_DOWNLOAD_HANDLE
#define DP_LOG_DB_LIMIT_ROWS 1000
#define DP_LOG_DB_CLEAR_LIMIT_ONE_TIME 100

// queue-manager
#define DP_MAX_DOWNLOAD_AT_ONCE 50

#endif
