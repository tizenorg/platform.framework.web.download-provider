/*
 * Copyright 2012  Samsung Electronics Co., Ltd
 *
 * Licensed under the Flora License, Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.tizenopensource.org/license
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * @file 	download-provider-network.cpp
 * @author	Jungki Kwak (jungki.kwak@samsung.com)
 * @brief	Download netowkr manager
 */

#include <stdlib.h>
#include "download-provider-common.h"
#include "download-provider-network.h"

enum {
	NET_INACTIVE = 0,
	NET_WIFI_ACTIVE,
	NET_CELLULAR_ACTIVE
};

NetMgr::NetMgr()
{
	m_netStatus = getConnectionState();
	if (m_netStatus == NET_WIFI_ACTIVE ||
			m_netStatus == NET_CELLULAR_ACTIVE) {
		getProxy();
		getIPAddress();
	}
}

NetMgr::~NetMgr()
{
}

void NetMgr::initNetwork()
{
	if (vconf_notify_key_changed(VCONFKEY_NETWORK_STATUS,
			netStatusChangedCB, NULL) < 0)
		DP_LOGE("Fail to register network status callback");
	if (vconf_notify_key_changed(VCONFKEY_NETWORK_CONFIGURATION_CHANGE_IND,
			netConfigChangedCB, NULL) < 0)
		DP_LOGE("Fail to register network config change callback");
}

void NetMgr::deinitNetwork()
{
	if (vconf_ignore_key_changed(VCONFKEY_NETWORK_STATUS,
			netStatusChangedCB) < 0)
		DP_LOGE("Fail to ignore network status callback");
	if (vconf_ignore_key_changed(VCONFKEY_NETWORK_CONFIGURATION_CHANGE_IND,
			netConfigChangedCB) < 0)
		DP_LOGE("Fail to ignore network config change callback");
}

int NetMgr::getConnectionState()
{
	int status = VCONFKEY_NETWORK_OFF;
	int ret = 0;

	ret = vconf_get_int(VCONFKEY_NETWORK_STATUS, &status);
	if (ret < 0) {
		DP_LOGE(" Fail to get network status : err[%d]",ret);
		return NET_INACTIVE;
	}
	switch (status) {
	case VCONFKEY_NETWORK_OFF:
		DP_LOG("VCONFKEY_NETWORK_OFF");
		ret = NET_INACTIVE;
		break;
	case VCONFKEY_NETWORK_CELLULAR:
		DP_LOG("VCONFKEY_NETWORK_CELLULAR");
		ret = getCellularStatus();
		break;
	case VCONFKEY_NETWORK_WIFI:
		DP_LOG("VCONFKEY_NETWORK_WIFI");
		ret = getWifiStatus();
		break;
	default:
		DP_LOGE("Cannot enter here");
		ret = NET_INACTIVE;
	}
	return ret;
}

int NetMgr::getCellularStatus()
{
	int status = VCONFKEY_NETWORK_CELLULAR_NO_SERVICE;
	int ret = 0;

	ret = vconf_get_int(VCONFKEY_NETWORK_CELLULAR_STATE, &status);

	if (ret < 0) {
		DP_LOGE(" Fail to get cellular status : err[%d]",ret);
		return NET_INACTIVE;
	}

	switch(status) {
	case VCONFKEY_NETWORK_CELLULAR_ON:
		DP_LOG("VCONFKEY_NETWORK_CELLULAR_ON");
		ret = NET_CELLULAR_ACTIVE;
		break;
	case VCONFKEY_NETWORK_CELLULAR_3G_OPTION_OFF:
		DP_LOG("VCONFKEY_NETWORK_CELLULAR_3G_OPTION_OFF");
		ret = NET_INACTIVE;
		break;
	case VCONFKEY_NETWORK_CELLULAR_ROAMING_OFF:
		DP_LOG("VCONFKEY_NETWORK_CELLULAR_ROAMING_OFF");
		ret = NET_INACTIVE;
		break;
	case VCONFKEY_NETWORK_CELLULAR_FLIGHT_MODE:
		DP_LOG("VCONFKEY_NETWORK_CELLULAR_FLIGHT_MODE");
		ret = NET_INACTIVE;
		break;
	case VCONFKEY_NETWORK_CELLULAR_NO_SERVICE:
		DP_LOG("VCONFKEY_NETWORK_CELLULAR_NO_SERVICE");
		ret = NET_INACTIVE;
		break;
	default:
		DP_LOGE("Cannot enter here");
		ret = NET_INACTIVE;
	}
	return ret;

}

int NetMgr::getWifiStatus()
{
	int status = VCONFKEY_NETWORK_WIFI_OFF;
	int ret = 0;

	ret = vconf_get_int(VCONFKEY_NETWORK_WIFI_STATE, &status);

	if (ret < 0) {
		DP_LOGE(" Fail to get wifi status : err[%d]",ret);
		return NET_INACTIVE;
	}

	switch(status) {
	case VCONFKEY_NETWORK_WIFI_CONNECTED:
		DP_LOG("VCONFKEY_NETWORK_WIFI_CONNECTED");
		ret = NET_WIFI_ACTIVE;
		break;
	case VCONFKEY_NETWORK_WIFI_NOT_CONNECTED:
		DP_LOG("VCONFKEY_NETWORK_WIFI_NOT_CONNECTED");
		ret = NET_INACTIVE;
		break;
	case VCONFKEY_NETWORK_WIFI_OFF:
		DP_LOG("VCONFKEY_NETWORK_WIFI_OFF");
		ret = NET_INACTIVE;
		break;
	default:
		DP_LOGE("Cannot enter here");
		ret = NET_INACTIVE;
	}
	return ret;
}

void NetMgr::netStatusChanged()
{
	int changedStatus = NET_INACTIVE;
	changedStatus = getConnectionState();
	DP_LOG("Previous[%d] Changed[%d]", m_netStatus, changedStatus);
	if (m_netStatus != changedStatus) {
		if (changedStatus == NET_INACTIVE)
			DP_LOG("Netowrk is disconnected");
		else
			DP_LOG("Network is connected");
		m_netStatus = changedStatus;
	} else {
		DP_LOGE("Network status is not changed. Cannot enter here");
	}
}

void NetMgr::netConfigChanged()
{
	int status = 0;
	int ret = 0;

	DP_LOG_FUNC();

	ret = vconf_get_int(VCONFKEY_NETWORK_CONFIGURATION_CHANGE_IND, &status);
	if (ret < 0) {
		DP_LOGE("Fail to get network configuration change ind : err[%d]",ret);
		return;
	}
	DP_LOG("netConfigChanged:status[%d]", status);


	if (status) { /* true */
		getProxy();
		getIPAddress();
		/* This notify is only for suspend event.
		 * If othere network event is added, it is needed to save event types
		 * and get function for it
		**/
		notify();
	} else {
		DP_LOGE("Network connection is disconnected");
	}
}

void NetMgr::getProxy()
{
	char *proxy = NULL;
	proxy = vconf_get_str(VCONFKEY_NETWORK_PROXY);
	if (proxy) {
		DP_LOG("===== Proxy address[%s] =====", proxy);
		free(proxy);
		proxy = NULL;
	}
}

void NetMgr::getIPAddress()
{
	char *ipAddr = NULL;
	ipAddr = vconf_get_str(VCONFKEY_NETWORK_IP);
	if (ipAddr) {
		DP_LOG("===== IP address[%s] =====", ipAddr);
		free(ipAddr);
		ipAddr= NULL;
	}
}

void NetMgr::netStatusChangedCB(keynode_t *keynode, void *data)
{
	NetMgr inst = NetMgr::getInstance();
	inst.netStatusChanged();
}

void NetMgr::netConfigChangedCB(keynode_t *keynode, void *data)
{
	NetMgr inst = NetMgr::getInstance();
	inst.netConfigChanged();
}

