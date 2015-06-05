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

#include "download-provider.h"
#include "download-provider-log.h"
#include "download-provider-pthread.h"
#include "download-provider-network.h"
#include "download-provider-client-manager.h"

#include <net_connection.h>

#ifdef SUPPORT_WIFI_DIRECT
#include <wifi-direct.h>
#endif

pthread_mutex_t g_dp_network_mutex = PTHREAD_MUTEX_INITIALIZER;
int g_network_status = DP_NETWORK_OFF;
connection_h g_network_connection = 0;
int g_network_is_wifi_direct = 0;

#ifdef SUPPORT_COMPANION_MODE
// support B3 companion mode
#include "vconf.h"
#include "vconf-keys.h"
#include "SAPInterface.h"
#define VCONFKEY_SAP_CONNECTION_NOTIFICATION VCONFKEY_WMS_WMANAGER_CONNECTED
#define VCONFKEY_SAP_CONNECTION_TYPE "memory/private/sap/conn_type"
#endif

static int __dp_network_is_companion_mode()
{
#ifdef SUPPORT_COMPANION_MODE
	int wms_connected = 0;
	int companion_mode = 0;
	vconf_get_int(VCONFKEY_SAP_CONNECTION_NOTIFICATION, &wms_connected);
	vconf_get_int(VCONFKEY_SAP_CONNECTION_TYPE, &companion_mode);
	TRACE_INFO("wms_connected:%d, companion_mode:%d", wms_connected, companion_mode);
	if (wms_connected == 1 && (companion_mode & SAP_BT))
		return 1;

#endif
	return 0;
}

#ifdef SUPPORT_WIFI_DIRECT

static int __dp_network_wifi_direct_status()
{
	int is_connected = 0;
	wifi_direct_state_e wifi_state = WIFI_DIRECT_STATE_DEACTIVATED;
	if (wifi_direct_get_state(&wifi_state) == 0) {
		if (wifi_state == WIFI_DIRECT_STATE_CONNECTED) {
			TRACE_INFO("WIFI_DIRECT_STATE_CONNECTED");
			is_connected = 1;
		}
	}
	return is_connected;
}

// support WIFI-Direct
static void __dp_network_wifi_direct_changed_cb
	(wifi_direct_error_e error_code,
	wifi_direct_connection_state_e connection_state,
	const char *mac_address, void *data)
{
	pthread_mutex_lock(&g_dp_network_mutex);
	if (connection_state == WIFI_DIRECT_CONNECTION_RSP) {
		TRACE_INFO("WIFI_DIRECT_CONNECTION_RSP");
		g_network_is_wifi_direct = __dp_network_wifi_direct_status();
	} else {
		TRACE_INFO("WIFI_DIRECT_DISCONNECTION");
		g_network_is_wifi_direct = 0;
	}
	pthread_mutex_unlock(&g_dp_network_mutex);
}
#endif

//////////////////////////////////////////////////////////////////////////
/// @brief		check the status in more detail by connection type
/// @return	dp_network_type
static int __dp_get_network_connection_status(connection_h connection, connection_type_e type)
{
	int network_type = DP_NETWORK_OFF;
	if (__dp_network_is_companion_mode() == 1) {
		network_type = DP_NETWORK_ALL;
		TRACE_INFO("COMPANION MODE");
	} else if (type == CONNECTION_TYPE_WIFI) {
		connection_wifi_state_e wifi_state;
		wifi_state = CONNECTION_WIFI_STATE_DEACTIVATED;
		if (connection_get_wifi_state(connection, &wifi_state) !=
				CONNECTION_ERROR_NONE) {
			TRACE_ERROR("Failed connection_get_wifi_state");
		} else {
			if (wifi_state == CONNECTION_WIFI_STATE_CONNECTED) {
				TRACE_INFO("WIFI CONNECTED");
				network_type = DP_NETWORK_WIFI;
			}
		}
	} else if (type == CONNECTION_TYPE_CELLULAR) {
		connection_cellular_state_e cellular_state;
		cellular_state = CONNECTION_CELLULAR_STATE_OUT_OF_SERVICE;
		if (connection_get_cellular_state(connection,
				&cellular_state) != CONNECTION_ERROR_NONE) {
			TRACE_ERROR("Failed connection_get_cellular_state");
		} else {
			if (cellular_state == CONNECTION_CELLULAR_STATE_CONNECTED) {
				TRACE_INFO("DATA NETWORK CONNECTED");
				network_type = DP_NETWORK_DATA_NETWORK;
			}
		}
	} else if (type == CONNECTION_TYPE_ETHERNET) {
		connection_ethernet_state_e ethernet_state;
		ethernet_state = CONNECTION_ETHERNET_STATE_DISCONNECTED;
		if (connection_get_ethernet_state(connection,
				&ethernet_state) != CONNECTION_ERROR_NONE) {
			TRACE_ERROR("Failed connection_get_ethernet_state");
		} else {
			if (ethernet_state == CONNECTION_ETHERNET_STATE_CONNECTED) {
				TRACE_INFO("ETHERNET CONNECTED");
				network_type = DP_NETWORK_WIFI;
			}
		}
	} else {
		TRACE_INFO("DISCONNECTED");
		network_type = DP_NETWORK_OFF;
	}
	g_network_status = network_type;
	return network_type;
}

//////////////////////////////////////////////////////////////////////////
/// @brief		[callback] called whenever changed network status
/// @todo		care requests by network status
static void __dp_network_connection_type_changed_cb(connection_type_e type, void *data)
{
	pthread_mutex_lock(&g_dp_network_mutex);
	// this callback guarantee that already connected
	if (__dp_network_is_companion_mode() == 1) {
		TRACE_INFO("COMPANION MODE");
		g_network_status = DP_NETWORK_ALL;
	} else if (type == CONNECTION_TYPE_WIFI) {
		TRACE_INFO("WIFI CONNECTED");
		g_network_status = DP_NETWORK_WIFI;
	} else if (type == CONNECTION_TYPE_CELLULAR) {
		TRACE_INFO("DATA NETWORK CONNECTED");
		g_network_status = DP_NETWORK_DATA_NETWORK;
	} else if (type == CONNECTION_TYPE_ETHERNET) {
		TRACE_INFO("ETHERNET CONNECTED");
		g_network_status = DP_NETWORK_WIFI;
	} else {
		TRACE_INFO("DISCONNECTED");
		g_network_status = DP_NETWORK_OFF;
	}
	pthread_mutex_unlock(&g_dp_network_mutex);
}

//////////////////////////////////////////////////////////////////////////
/// @brief		[callback] called when changed network ip
/// @todo		auto resume feature
static void __dp_network_connection_ip_changed_cb(const char *ip, const char *ipv6, void *data)
{
	if (dp_network_get_status() != DP_NETWORK_OFF) {
		TRACE_DEBUG("[CONNECTION] IP CHANGED");
		// broadcast to all thread for clients
		dp_broadcast_signal();
	}
}

int dp_network_get_status()
{
	int status = DP_NETWORK_OFF;
	pthread_mutex_lock(&g_dp_network_mutex);
	status = g_network_status;
	pthread_mutex_unlock(&g_dp_network_mutex);
	return status;
}

int dp_network_is_wifi_direct()
{
	pthread_mutex_lock(&g_dp_network_mutex);
	int status = g_network_is_wifi_direct;
	pthread_mutex_unlock(&g_dp_network_mutex);
	return status;
}

//////////////////////////////////////////////////////////////////////////
/// @brief		create connection handle & regist callback
/// @return	0 : success -1 : failed
int dp_network_connection_init()
{
	int retcode = 0;

#ifdef SUPPORT_WIFI_DIRECT
	if (wifi_direct_initialize() == 0) {
		wifi_direct_set_connection_state_changed_cb
			(__dp_network_wifi_direct_changed_cb, NULL);
		g_network_is_wifi_direct = __dp_network_wifi_direct_status();
	}
#endif

	if ((retcode = connection_create(&g_network_connection)) !=
			CONNECTION_ERROR_NONE) {
		TRACE_ERROR("Failed connection_create [%d]", retcode);
		return -1;
	}
	if ((retcode = connection_set_type_changed_cb(g_network_connection,
			__dp_network_connection_type_changed_cb, NULL)) !=
				CONNECTION_ERROR_NONE) {
		TRACE_ERROR("Failed connection_set_type_changed_cb [%d]", retcode);
		connection_destroy(g_network_connection);
		g_network_connection = 0;
		return -1;
	}

	if ((retcode = connection_set_ip_address_changed_cb
			(g_network_connection, __dp_network_connection_ip_changed_cb,
				NULL)) != CONNECTION_ERROR_NONE) {
		TRACE_ERROR("Failed __dp_network_connection_ip_changed_cb [%d]", retcode);
		connection_destroy(g_network_connection);
		g_network_connection = 0;
		return -1;
	}

	connection_type_e type = CONNECTION_TYPE_DISCONNECTED;
	if ((retcode = connection_get_type(g_network_connection, &type)) !=
			CONNECTION_ERROR_NONE) {
		TRACE_ERROR("Failed connection_get_type [%d]", retcode);
		connection_destroy(g_network_connection);
		g_network_connection = 0;
		return -1;
	}
	g_network_status =
		__dp_get_network_connection_status(g_network_connection, type);
	return 0;
}

//////////////////////////////////////////////////////////////////////////
/// @brief		destroy connection handle
void dp_network_connection_destroy()
{
	pthread_mutex_lock(&g_dp_network_mutex);
#ifdef SUPPORT_WIFI_DIRECT
	wifi_direct_unset_connection_state_changed_cb();
	wifi_direct_deinitialize();
#endif

	if (g_network_connection != 0) {
		connection_unset_type_changed_cb(g_network_connection);
		connection_unset_ip_address_changed_cb(g_network_connection);
		connection_destroy(g_network_connection);
	}
	g_network_connection = 0;
	pthread_mutex_unlock(&g_dp_network_mutex);
}
