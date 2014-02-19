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

#ifndef DOWNLOAD_PROVIDER2_NETWORK_H
#define DOWNLOAD_PROVIDER2_NETWORK_H

#include <net_connection.h>

#include "download-provider.h"

dp_network_type dp_get_network_connection_status(connection_h connection, connection_type_e type);
void dp_network_connection_type_changed_cb(connection_type_e type, void *data);
int dp_network_connection_init(dp_privates *privates);
void dp_network_connection_destroy(connection_h connection);
dp_network_type dp_get_network_connection_instant_status();

#ifdef SUPPORT_WIFI_DIRECT
int dp_network_wifi_direct_is_connected();
#endif

#endif
