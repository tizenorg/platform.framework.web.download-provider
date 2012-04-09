/*
 * Copyright (c) 2000 - 2012 Samsung Electronics Co., Ltd All Rights Reserved 
 *
 * This file is part of Download Provider
 * Written by Jungki Kwak <jungki.kwak@samsung.com>
 *
 * PROPRIETARY/CONFIDENTIAL
 *
 * This software is the confidential and proprietary information of SAMSUNG ELECTRONICS
 * ("Confidential Information"). You shall not disclose such Confidential Information
 * and shall use it only in accordance with the terms of the license agreement
 * you entered into with SAMSUNG ELECTRONICS.
 *
 * SAMSUNG make no representations or warranties about the suitability of the software,
 * either express or implied, including but not limited to the implied warranties of merchantability,
 * fitness for a particular purpose, or non-infringement.
 *
 * SAMSUNG shall not be liable for any damages suffered by licensee as a result of using,
 * modifying or distributing this software or its derivatives.
 */
/**
 * @file 	download-provider-network.h
 * @author	Jungki Kwak (jungki.kwak@samsung.com)
 * @brief	Download netowkr manager
 */

#ifndef DOWNLOAD_PROVIDER_NETWORK_H
#define DOWNLOAD_PROVIDER_NETWORK_H

#include "vconf.h"
#include "vconf-keys.h"
#include "download-provider-event.h"

class NetMgr {
public:
	static NetMgr& getInstance(void) {
		static NetMgr inst;
		return inst;
	}
	void initNetwork(void);
	void deinitNetwork(void);
	void netStatusChanged(void);
	void netConfigChanged(void);
	inline void subscribe(Observer *o) { m_subject.attach(o); }
	inline void deSubscribe(Observer *o) { m_subject.detach(o); }
	static void netStatusChangedCB(keynode_t *keynode, void *data);
	static void netConfigChangedCB(keynode_t *keynode, void *data);
private:
	NetMgr(void);
	~NetMgr(void);
	int getConnectionState(void);
	int getCellularStatus(void);
	int getWifiStatus(void);
	void getProxy(void);
	void getIPAddress(void);
	inline void notify() { m_subject.notify(); }
	int m_netStatus;
	Subject m_subject;
};

#endif
