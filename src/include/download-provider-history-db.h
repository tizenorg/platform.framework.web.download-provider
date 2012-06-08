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
 * @file 	download-provider-history-db.h
 * @author	Jungki Kwak (jungki.kwak@samsung.com)
 * @brief	Manager for a download history DB
 */

#ifndef DOWNLOAD_PROVIDER_HISTORY_DB_H
#define DOWNLOAD_PROVIDER_HISTORY_DB_H

#include <string>
#include <queue>
#include <db-util.h>
#include "download-provider-item.h"
extern "C" {
#include <unicode/utypes.h>
}

using namespace std;

class DownloadHistoryDB
{
public:
	static bool addToHistoryDB(Item *item);
	static bool createRemainedItemsFromHistoryDB(int limit, int offset);
	static bool createItemsFromHistoryDB(void);
	static bool deleteItem(unsigned int historyId);
	static bool deleteMultipleItem(queue <unsigned int> &q);
	static bool clearData(void);
	static bool getCountOfHistory(int *count);
private:
	DownloadHistoryDB(void);
	~DownloadHistoryDB(void);
	static sqlite3* historyDb;
	static bool open(void);
	static bool isOpen(void) { return historyDb ? true : false; }
	static void close(void);
};

#endif	/* DOWNLOAD_PROVIDER_HISTORY_DB_H */

