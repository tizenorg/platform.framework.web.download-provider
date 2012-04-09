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
 * @file	download-provider-items.cpp
 * @author	Jungki Kwak (jungki.kwak@samsung.com)
 * @brief	Interface API with item list
 */
#include "download-provider-items.h"

void Items::attachItem(Item *item)
{
	m_items.push_back(item);
}

void Items::detachItem(Item *item)
{
	vector<Item *>::iterator it;
	for (it = m_items.begin() ; it < m_items.end() ; it++) {
		if (*it == item) {
			m_items.erase(it);
			delete item;
		}
	}
}

bool Items::isExistedHistoryId(unsigned int id)
{
	vector <Item *>::iterator it;
	for (it = m_items.begin(); it < m_items.end(); it++) {
		if ((*it)->historyId() == id ) {
			DP_LOGD("historyId[%ld],title[%s]",
				(*it)->historyId(), (*it)->title().c_str());
			return true;
		}
	}
	return false;
}

