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
