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

#ifndef DOWNLOAD_PROVIDER_NETWORK_H
#define DOWNLOAD_PROVIDER_NETWORK_H

#define DP_NETWORK_OFF -1

typedef enum {
	DP_NETWORK_DATA_NETWORK = 0,
	DP_NETWORK_WIFI = 1,
	DP_NETWORK_WIFI_DIRECT = 2,
	DP_NETWORK_ALL = 3
} dp_network_defs;

int dp_network_connection_init();
void dp_network_connection_destroy();
int dp_network_get_status();
int dp_network_is_wifi_direct();

#endif
