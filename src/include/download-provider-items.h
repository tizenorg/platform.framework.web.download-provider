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
/*
 * @file	download-provider-items.h
 * @author	Jungki Kwak (jungki.kwak@samsung.com)
 * @brief   item inventory class
 */
#ifndef DOWNLOAD_PROVIDER_ITEMS_H
#define DOWNLOAD_PROVIDER_ITEMS_H

#include "download-provider-item.h"
#include <vector>

class Items {
public:
	static Items& getInstance(void) {
		static Items inst;
		return inst;
	}

	void attachItem(Item *item);
	void detachItem(Item *item);
	bool isExistedHistoryId(unsigned int id);
private:
	Items(){}
	~Items(){DP_LOGD_FUNC();}

	vector<Item*> m_items;
};

#endif /* DOWNLOAD_PROVIDER_ITEMS_H */
