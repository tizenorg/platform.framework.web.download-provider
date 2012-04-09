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

