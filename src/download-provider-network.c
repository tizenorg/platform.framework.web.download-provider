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

#include "download-provider-log.h"
#include "download-provider-config.h"
#include "download-provider-pthread.h"
#include "download-provider-network.h"

extern pthread_mutex_t g_dp_queue_mutex;
extern pthread_cond_t g_dp_queue_cond;

#if 0
typedef enum
{
    CONNECTION_TYPE_DISCONNECTED = 0,  /**< Disconnected */
    CONNECTION_TYPE_WIFI = 1,  /**< Wi-Fi type */
    CONNECTION_TYPE_CELLULAR = 2,  /**< Cellular type */
    CONNECTION_TYPE_ETHERNET = 3,  /**< Ethernet type */
    CONNECTION_TYPE_BT = 4,  /**< Bluetooth type */
} connection_type_e;
typedef enum
{
    CONNECTION_CELLULAR_STATE_OUT_OF_SERVICE = 0,  /**< Out of service */
    CONNECTION_CELLULAR_STATE_FLIGHT_MODE = 1,  /**< Flight mode */
    CONNECTION_CELLULAR_STATE_ROAMING_OFF = 2,  /**< Roaming is turned off */
    CONNECTION_CELLULAR_STATE_CALL_ONLY_AVAILABLE = 3,  /**< Call is only available */
    CONNECTION_CELLULAR_STATE_AVAILABLE = 4,  /**< Available but not connected yet */
    CONNECTION_CELLULAR_STATE_CONNECTED = 5,  /**< Connected */
} connection_cellular_state_e
typedef enum
{
    CONNECTION_WIFI_STATE_DEACTIVATED = 0,  /**< Deactivated state */
    CONNECTION_WIFI_STATE_DISCONNECTED = 1,  /**< disconnected state */
    CONNECTION_WIFI_STATE_CONNECTED = 2,  /**< Connected state */
} connection_wifi_state_e;
typedef enum
{
    CONNECTION_ETHERNET_STATE_DEACTIVATED = 0,  /**< Deactivated state */
    CONNECTION_ETHERNET_STATE_DISCONNECTED = 1,  /**< disconnected state */
    CONNECTION_ETHERNET_STATE_CONNECTED = 2,  /**< Connected state */
} connection_ethernet_state_e;
typedef enum
{
    CONNECTION_ERROR_NONE = TIZEN_ERROR_NONE, /**< Successful */
    CONNECTION_ERROR_INVALID_PARAMETER = TIZEN_ERROR_INVALID_PARAMETER, /**< Invalid parameter */
    CONNECTION_ERROR_OUT_OF_MEMORY = TIZEN_ERROR_OUT_OF_MEMORY, /**< Out of memory error */
    CONNECTION_ERROR_INVALID_OPERATION = TIZEN_ERROR_INVALID_OPERATION, /**< Invalid Operation */
    CONNECTION_ERROR_ADDRESS_FAMILY_NOT_SUPPORTED = TIZEN_ERROR_ADDRESS_FAMILY_NOT_SUPPORTED, /**< Address family not supported */
    CONNECTION_ERROR_OPERATION_FAILED = TIZEN_ERROR_NETWORK_CLASS|0x0401, /**< Operation failed */
    CONNECTION_ERROR_ITERATOR_END = TIZEN_ERROR_NETWORK_CLASS|0x0402, /**< End of iteration */
    CONNECTION_ERROR_NO_CONNECTION = TIZEN_ERROR_NETWORK_CLASS|0x0403, /**< There is no connection */
    CONNECTION_ERROR_NOW_IN_PROGRESS = TIZEN_ERROR_NOW_IN_PROGRESS, /** Now in progress */
    CONNECTION_ERROR_ALREADY_EXISTS = TIZEN_ERROR_NETWORK_CLASS|0x0404, /**< Already exists */
    CONNECTION_ERROR_OPERATION_ABORTED = TIZEN_ERROR_NETWORK_CLASS|0x0405, /**< Operation is aborted */
    CONNECTION_ERROR_DHCP_FAILED = TIZEN_ERROR_NETWORK_CLASS|0x0406, /**< DHCP failed  */
    CONNECTION_ERROR_INVALID_KEY = TIZEN_ERROR_NETWORK_CLASS|0x0407, /**< Invalid key  */
    CONNECTION_ERROR_NO_REPLY = TIZEN_ERROR_NETWORK_CLASS|0x0408, /**< No reply */
} connection_error_e;


static void __print_connection_errorcode_to_string(connection_error_e errorcode)
{
	switch(errorcode)
	{
		case CONNECTION_ERROR_INVALID_PARAMETER :
			TRACE_INFO("CONNECTION_ERROR_INVALID_PARAMETER");
			break;
		case CONNECTION_ERROR_OUT_OF_MEMORY :
			TRACE_INFO("CONNECTION_ERROR_OUT_OF_MEMORY");
			break;
		case CONNECTION_ERROR_INVALID_OPERATION :
			TRACE_INFO("CONNECTION_ERROR_INVALID_OPERATION");
			break;
		case CONNECTION_ERROR_ADDRESS_FAMILY_NOT_SUPPORTED :
			TRACE_INFO("CONNECTION_ERROR_ADDRESS_FAMILY_NOT_SUPPORTED");
			break;
		case CONNECTION_ERROR_OPERATION_FAILED :
			TRACE_INFO("CONNECTION_ERROR_OPERATION_FAILED");
			break;
		case CONNECTION_ERROR_ITERATOR_END :
			TRACE_INFO("CONNECTION_ERROR_ITERATOR_END");
			break;
		case CONNECTION_ERROR_NO_CONNECTION :
			TRACE_INFO("CONNECTION_ERROR_NO_CONNECTION");
			break;
		case CONNECTION_ERROR_NOW_IN_PROGRESS :
			TRACE_INFO("CONNECTION_ERROR_NOW_IN_PROGRESS");
			break;
		case CONNECTION_ERROR_ALREADY_EXISTS :
			TRACE_INFO("CONNECTION_ERROR_ALREADY_EXISTS");
			break;
		case CONNECTION_ERROR_OPERATION_ABORTED :
			TRACE_INFO("CONNECTION_ERROR_OPERATION_ABORTED");
			break;
		case CONNECTION_ERROR_DHCP_FAILED :
			TRACE_INFO("CONNECTION_ERROR_DHCP_FAILED");
			break;
		case CONNECTION_ERROR_INVALID_KEY :
			TRACE_INFO("CONNECTION_ERROR_INVALID_KEY");
			break;
		case CONNECTION_ERROR_NO_REPLY :
			TRACE_INFO("CONNECTION_ERROR_NO_REPLY");
			break;
		default :
			TRACE_INFO("CONNECTION_ERROR_NONE");
			break;
	}
}
#endif







//////////////////////////////////////////////////////////////////////////
/// @brief		check the status in more detail by connection type
/// @return	dp_network_type
static dp_network_type __dp_get_network_connection_status(connection_h connection, connection_type_e type)
{
	dp_network_type network_type = DP_NETWORK_TYPE_OFF;
	if (type == CONNECTION_TYPE_WIFI) {
		connection_wifi_state_e wifi_state;
		wifi_state = CONNECTION_WIFI_STATE_DEACTIVATED;
		if (connection_get_wifi_state
			(connection, &wifi_state) != CONNECTION_ERROR_NONE)
			TRACE_ERROR("Failed connection_get_wifi_state");
		if (wifi_state == CONNECTION_WIFI_STATE_CONNECTED) {
			TRACE_INFO("[CONNECTION_WIFI] CONNECTED");
			network_type = DP_NETWORK_TYPE_WIFI;
		} else {
			TRACE_INFO("[CONNECTION_WIFI] [%d]", wifi_state);
		}
	} else if (type == CONNECTION_TYPE_CELLULAR) {
		connection_cellular_state_e cellular_state;
		cellular_state = CONNECTION_CELLULAR_STATE_OUT_OF_SERVICE;
		if (connection_get_cellular_state
			(connection, &cellular_state) != CONNECTION_ERROR_NONE)
			TRACE_ERROR("Failed connection_get_cellular_state");
		if (cellular_state == CONNECTION_CELLULAR_STATE_CONNECTED) {
			TRACE_INFO("[CONNECTION_CELLULAR] DATA NETWORK CONNECTED");
			network_type = DP_NETWORK_TYPE_DATA_NETWORK;
		} else {
			TRACE_INFO("[CONNECTION_CELLULAR] [%d]", cellular_state);
		}
	} else if (type == CONNECTION_TYPE_ETHERNET) {
		connection_ethernet_state_e ethernet_state;
		ethernet_state = CONNECTION_ETHERNET_STATE_DISCONNECTED;
		if (connection_get_ethernet_state
			(connection, &ethernet_state) != CONNECTION_ERROR_NONE)
			TRACE_ERROR("Failed connection_get_ethernet_state");
		if (ethernet_state == CONNECTION_ETHERNET_STATE_CONNECTED) {
			TRACE_INFO("[CONNECTION_ETHERNET] ETHERNET CONNECTED");
			network_type = DP_NETWORK_TYPE_ETHERNET;
		} else {
			TRACE_INFO("[CONNECTION_ETHERNET] [%d]", ethernet_state);
		}
	} else {
		TRACE_INFO("[DISCONNECTED]");
		network_type = DP_NETWORK_TYPE_OFF;
	}
	return network_type;
}

//////////////////////////////////////////////////////////////////////////
/// @brief		[callback] called whenever changed network status
/// @todo		care requests by network status
static void __dp_network_connection_type_changed_cb(connection_type_e type, void *data)
{
	TRACE_INFO("type[%d]", type);
	dp_privates *privates = (dp_privates*)data;
	if (!privates) {
		TRACE_ERROR("[CRITICAL] Invalid data");
		return ;
	}
	CLIENT_MUTEX_LOCK(&(g_dp_queue_mutex));
	#if 1 // this callback guarantee that already connectdd
	if (type == CONNECTION_TYPE_WIFI) {
		TRACE_INFO("[CONNECTION_WIFI] CONNECTED");
		privates->network_status = DP_NETWORK_TYPE_WIFI;
	} else if (type == CONNECTION_TYPE_CELLULAR) {
		TRACE_INFO("[CONNECTION_CELLULAR] DATA NETWORK CONNECTED");
		privates->network_status = DP_NETWORK_TYPE_DATA_NETWORK;
	} else if (type == CONNECTION_TYPE_ETHERNET) {
		TRACE_INFO("[CONNECTION_ETHERNET] ETHERNET CONNECTED");
		privates->network_status = DP_NETWORK_TYPE_ETHERNET;
	} else {
		TRACE_INFO("[DISCONNECTED]");
		privates->network_status = DP_NETWORK_TYPE_OFF;
	}
	if (privates->network_status != DP_NETWORK_TYPE_OFF)
		pthread_cond_signal(&g_dp_queue_cond);
	#else
	privates->network_status =
		__dp_get_network_connection_status(privates->connection, type);
	#endif
	CLIENT_MUTEX_UNLOCK(&(g_dp_queue_mutex));
}

//////////////////////////////////////////////////////////////////////////
/// @brief		create connection handle & regist callback
/// @return	0 : success -1 : failed
int dp_network_connection_init(dp_privates *privates)
{
	int retcode = 0;

	TRACE_INFO("");
	if (!privates) {
		TRACE_ERROR("[CRITICAL] Invalid data");
		return -1;
	}
	if ((retcode = connection_create(&privates->connection)) !=
			CONNECTION_ERROR_NONE) {
		TRACE_ERROR("Failed connection_create [%d]", retcode);
		return -1;
	}
	if ((retcode = connection_set_type_changed_cb
			(privates->connection, __dp_network_connection_type_changed_cb,
				privates)) != CONNECTION_ERROR_NONE) {
		TRACE_ERROR("Failed connection_set_type_changed_cb [%d]", retcode);
		connection_destroy(privates->connection);
		return -1;
	}
	connection_type_e type = CONNECTION_TYPE_DISCONNECTED;
	if ((retcode = connection_get_type(privates->connection, &type)) !=
			CONNECTION_ERROR_NONE) {
		TRACE_ERROR("Failed connection_get_type [%d]", retcode);
		connection_destroy(privates->connection);
		return -1;
	}
	privates->network_status =
		__dp_get_network_connection_status(privates->connection, type);
	return 0;
}

//////////////////////////////////////////////////////////////////////////
/// @brief		destroy connection handle
void dp_network_connection_destroy(connection_h connection)
{
	TRACE_INFO("");
	connection_unset_type_changed_cb (connection);
	connection_destroy(connection);
}

//////////////////////////////////////////////////////////////////////////
/// @brief		check network status using connection API
/// @todo		the standard of enabled networking can be changed later
/// @return	Network type
dp_network_type dp_get_network_connection_instant_status()
{
	int retcode = 0;
	connection_h network_handle = NULL;
	dp_network_type network_type = DP_NETWORK_TYPE_OFF;
	if ((retcode = connection_create(&network_handle)) !=
			CONNECTION_ERROR_NONE) {
		TRACE_ERROR("Failed connection_create [%d]", retcode);
		return DP_NETWORK_TYPE_OFF;
	}

	connection_type_e type = CONNECTION_TYPE_DISCONNECTED;
	if ((retcode = connection_get_type(network_handle, &type)) !=
			CONNECTION_ERROR_NONE) {
		TRACE_ERROR("Failed connection_get_type [%d]", retcode);
		connection_destroy(network_handle);
		return DP_NETWORK_TYPE_OFF;
	}
	network_type =
		__dp_get_network_connection_status(network_handle, type);

	if (connection_destroy(network_handle) != CONNECTION_ERROR_NONE)
		TRACE_ERROR("Failed connection_destroy");

	return network_type;
}
