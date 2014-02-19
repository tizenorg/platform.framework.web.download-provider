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

#ifndef DOWNLOAD_PROVIDER2_CONFIG_H
#define DOWNLOAD_PROVIDER2_CONFIG_H

#include <download-provider.h>
#include <download-provider-slots.h>

#include <net_connection.h>

#define DP_LOCK_PID "/tmp/download-provider.lock"

#define DP_CARE_CLIENT_MIN_INTERVAL 5
#define DP_CARE_CLIENT_MAX_INTERVAL 120

// check this value should be lower than DP_MAX_REQUEST
#define DP_MAX_DOWNLOAD_AT_ONCE 50

#define DP_LOG_DB_LIMIT_ROWS 1000
#define DP_LOG_DB_CLEAR_LIMIT_ONE_TIME 100

// Share the structure for all threads
typedef struct {
	int listen_fd;
	int is_connected_wifi_direct;
	connection_h connection;
	dp_network_type network_status;
	dp_group_slots *groups;
	dp_request_slots *requests;
} dp_privates;

#endif
