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
 * @file 	download-provider-history-db.cpp
 * @author	Jungki Kwak (jungki.kwak@samsung.com)
 * @brief	Manager for a download history DB
 */

#include <sstream>
#include "download-provider-common.h"
#include "download-provider-history-db.h"

#define FINALIZE_ON_ERROR( stmt ) { \
	DP_LOG("SQL error: %d", ret);\
	if (sqlite3_finalize(stmt) != SQLITE_OK)\
		DP_LOGE("sqlite3_finalize is failed.");\
	close();\
	return false; \
}

sqlite3 *DownloadHistoryDB::historyDb = NULL;

DownloadHistoryDB::DownloadHistoryDB()
{
}

DownloadHistoryDB::~DownloadHistoryDB()
{
}

bool DownloadHistoryDB::open()
{
	int ret = 0;

	DP_LOGD_FUNC();

	close();

	ret = db_util_open(DBDATADIR"/"HISTORYDB, &historyDb,
		DB_UTIL_REGISTER_HOOK_METHOD);

	if (ret != SQLITE_OK) {
		DP_LOGE("open fail");
		db_util_close(historyDb);
		historyDb = NULL;
		return false;
	}

	return isOpen();
}

void DownloadHistoryDB::close()
{
	DP_LOGD_FUNC();
	if (historyDb) {
		db_util_close(historyDb);
		historyDb = NULL;
	}
}

/* FIXME : Hitory entry limitation ?? */
bool DownloadHistoryDB::addToHistoryDB(Item *item)
{
	int ret = 0;
	sqlite3_stmt *stmt = NULL;
	const char *name = NULL;

	DP_LOG_FUNC();

	if (!item) {
		DP_LOGE("Item is NULL");
		return false;
	}

	if (item->historyId() < 0) {
		DP_LOGE("Cannot add to DB. Because historyId is invaild");
		return false;
	}

	if (!open()) {
		DP_LOGE("historyDB is NULL");
		return false;
	}

	const string statement = "insert into history (historyid, downloadtype,\
		contenttype, state, err, name, vendor, path, url, cookie, date) \
		values(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";

	ret = sqlite3_prepare_v2(historyDb, statement.c_str(), -1, &stmt, NULL);

	if (ret != SQLITE_OK)
		FINALIZE_ON_ERROR(stmt);
	/* binding values */
	if (sqlite3_bind_int(stmt, 1, item->historyId()) != SQLITE_OK)
		DP_LOGE("sqlite3_bind_int is failed.");
	if (sqlite3_bind_int(stmt, 2, item->downloadType()) != SQLITE_OK)
		DP_LOGE("sqlite3_bind_int is failed.");
	if (sqlite3_bind_int(stmt, 3, item->contentType()) != SQLITE_OK)
		DP_LOGE("sqlite3_bind_int is failed.");
	if (sqlite3_bind_int(stmt, 4, item->state()) != SQLITE_OK)
		DP_LOGE("sqlite3_bind_int is failed.");
	if (sqlite3_bind_int(stmt, 5, item->errorCode()) != SQLITE_OK)
		DP_LOGE("sqlite3_bind_int is failed.");
	if (item->downloadType() == DL_TYPE::MIDP_DOWNLOAD)
		name = item->contentName().c_str();
	else
		name = item->title().c_str();
	if (sqlite3_bind_text(stmt, 6, name, -1, NULL) != SQLITE_OK)
		DP_LOGE("sqlite3_bind_text is failed.");
	if (sqlite3_bind_text(stmt, 7, item->vendorName().c_str(), -1, NULL) !=
			SQLITE_OK)
		DP_LOGE("sqlite3_bind_int is failed.");
	if (sqlite3_bind_text(
			stmt, 8, item->registeredFilePath().c_str(), -1, NULL) != SQLITE_OK)
		DP_LOGE("sqlite3_bind_text is failed.");
	if (sqlite3_bind_text(stmt, 9, item->url().c_str(), -1, NULL) != SQLITE_OK)
		DP_LOGE("sqlite3_bind_text is failed.");
	if (sqlite3_bind_text(stmt, 10, item->cookie().c_str(), -1, NULL) != SQLITE_OK)
		DP_LOGE("sqlite3_bind_text is failed.");
	if (sqlite3_bind_double(stmt, 11, item->finishedTime()) != SQLITE_OK)
		DP_LOGE("sqlite3_bind_double is failed.");
	ret = sqlite3_step(stmt);

	DP_LOGD("SQL return: %s", (ret == SQLITE_ROW || ret == SQLITE_OK)?"Success":"Fail");

	close();

	return ret == SQLITE_DONE;
}

bool DownloadHistoryDB::getCountOfHistory(int *count)
{
	int ret = 0;
	sqlite3_stmt *stmt = NULL;

	DP_LOG_FUNC();

	if (!open()) {
		DP_LOGE("historyDB is NULL");
		return false;
	}
	ret = sqlite3_prepare_v2(historyDb, "select COUNT(*) from history", -1, &stmt, NULL);
	if (ret != SQLITE_OK)
		FINALIZE_ON_ERROR(stmt);

	ret = sqlite3_step(stmt);
	DP_LOGD("SQL return: %s", (ret == SQLITE_ROW || ret == SQLITE_OK)?"Success":"Fail");
	if (ret == SQLITE_ROW) {
		*count = sqlite3_column_int(stmt,0);
		DP_LOGD("count[%d]",*count);
	} else {
		DP_LOGE("SQL query error");
		*count = 0;
	}

	if (sqlite3_finalize(stmt) != SQLITE_OK)
		DP_LOGE("sqlite3_finalize is failed.");
	close();
	return true;
}

bool DownloadHistoryDB::createRemainedItemsFromHistoryDB(int limit, int offset)
{
	int ret = 0;
	stringstream limitStr;
	stringstream offsetStr;
	sqlite3_stmt *stmt = NULL;
	string tmp;

	DP_LOG_FUNC();

	if (!open()) {
		DP_LOGE("historyDB is NULL");
		return false;
	}
	limitStr << limit;
	offsetStr << offset;

	tmp.append("select historyid, downloadtype, contenttype, state, err, ");
	tmp.append("name, vendor, path, url, cookie, date from history order by ");
	tmp.append("date DESC limit ");
	tmp.append(limitStr.str());
	tmp.append(" offset ");
	tmp.append(offsetStr.str());
	ret = sqlite3_prepare_v2(historyDb, tmp.c_str(), -1, &stmt, NULL);
	if (ret != SQLITE_OK)
		FINALIZE_ON_ERROR(stmt);

	for (;;) {
		ret = sqlite3_step(stmt);
		if (ret == SQLITE_ROW) {
			const char *tempStr = NULL;
			string arg;
			string url;
			string cookie;
			Item *item = Item::createHistoryItem();
			if (!item) {
				DP_LOGE("Fail to create item");
				break;
			}
			item->setHistoryId(sqlite3_column_int(stmt,0));
			item->setDownloadType((DL_TYPE::TYPE)sqlite3_column_int(stmt,1));
			item->setContentType(sqlite3_column_int(stmt,2));
			item->setState((ITEM::STATE)sqlite3_column_int(stmt,3));
			item->setErrorCode((ERROR::CODE)sqlite3_column_int(stmt,4));
			tempStr = (const char *)(sqlite3_column_text(stmt,5));
			if (tempStr) {
				arg = tempStr;
				if (item->downloadType() == DL_TYPE::MIDP_DOWNLOAD)
					item->setContentName(arg);
				item->setTitle(arg);
			}
			tempStr = (const char *)(sqlite3_column_text(stmt,6));
			if (tempStr)
				arg = tempStr;
			else
				arg = string();
			item->setVendorName(arg);
			tempStr = (const char *)(sqlite3_column_text(stmt,7));
			if (tempStr)
				arg = tempStr;
			else
				arg = string();
			item->setRegisteredFilePath(arg);
			tempStr = (const char *)(sqlite3_column_text(stmt,8));
			if (tempStr)
				url = tempStr;
			else
				url = string();
			tempStr = (const char *)(sqlite3_column_text(stmt,9));
			if (tempStr)
				cookie = tempStr;
			else
				cookie = string();
			item->setFinishedTime(sqlite3_column_double(stmt,10));
			item->attachHistoryItem();
			item->setRetryData(url, cookie);
		} else
			break;
	}
	DP_LOGD("SQL error: %d", ret);

	if (sqlite3_finalize(stmt) != SQLITE_OK)
		DP_LOGE("sqlite3_finalize is failed.");

	close();

	if (ret == SQLITE_DONE || ret == SQLITE_ROW)
		return true;
	else
		return false;
}

bool DownloadHistoryDB::createItemsFromHistoryDB()
{
	int ret = 0;
	sqlite3_stmt *stmt = NULL;
	string tmp;
	stringstream limitStr;

	DP_LOG_FUNC();

	if (!open()) {
		DP_LOGE("historyDB is NULL");
		return false;
	}
	limitStr << LOAD_HISTORY_COUNT;
	tmp.append("select historyid, downloadtype, contenttype, state, err, ");
	tmp.append("name, vendor, path, url, cookie, date from history order by ");
	tmp.append("date DESC limit ");
	tmp.append(limitStr.str());
	ret = sqlite3_prepare_v2(historyDb, tmp.c_str(), -1, &stmt, NULL);
	if (ret != SQLITE_OK)
		FINALIZE_ON_ERROR(stmt);

	for (;;) {
		ret = sqlite3_step(stmt);
		if (ret == SQLITE_ROW) {
			const char *tempStr = NULL;
			string arg = string();
			string url = string();
			string cookie = string();
			Item *item = Item::createHistoryItem();
			if (!item) {
				DP_LOGE("Fail to create item");
				break;
			}
			item->setHistoryId(sqlite3_column_int(stmt,0));
			item->setDownloadType((DL_TYPE::TYPE)sqlite3_column_int(stmt,1));
			item->setContentType(sqlite3_column_int(stmt,2));
			item->setState((ITEM::STATE)sqlite3_column_int(stmt,3));
			item->setErrorCode((ERROR::CODE)sqlite3_column_int(stmt,4));
			tempStr = (const char *)(sqlite3_column_text(stmt,5));
			if (tempStr) {
				arg = tempStr;
				if (item->downloadType() == DL_TYPE::MIDP_DOWNLOAD)
					item->setContentName(arg);
				item->setTitle(arg);
			}
			tempStr = (const char *)(sqlite3_column_text(stmt,6));
			if (tempStr)
				arg = tempStr;
			item->setVendorName(arg);
			tempStr = (const char *)(sqlite3_column_text(stmt,7));
			if (tempStr)
				arg = tempStr;
			item->setRegisteredFilePath(arg);
			tempStr = (const char *)(sqlite3_column_text(stmt,8));
			if (tempStr)
				url = tempStr;
			tempStr = (const char *)(sqlite3_column_text(stmt,9));
			if (tempStr)
				cookie = tempStr;
			item->setFinishedTime(sqlite3_column_double(stmt,10));
			item->attachHistoryItem();
			item->setRetryData(url, cookie);
		} else
			break;
	}
	DP_LOGD("SQL error: %d", ret);

	if (sqlite3_finalize(stmt) != SQLITE_OK)
		DP_LOGE("sqlite3_finalize is failed.");
	close();

	if (ret == SQLITE_DONE || ret == SQLITE_ROW)
		return true;
	else
		return false;
}

bool DownloadHistoryDB::deleteItem(unsigned int historyId)
{
	int ret = 0;
	sqlite3_stmt *stmt = NULL;

	DP_LOG_FUNC();

	if (!open()) {
		DP_LOGE("historyDB is NULL");
		return false;
	}

	ret = sqlite3_prepare_v2(historyDb, "delete from history where historyid=?",
		-1, &stmt, NULL);

	if (ret != SQLITE_OK)
		FINALIZE_ON_ERROR(stmt);
	if (sqlite3_bind_int(stmt, 1, historyId) != SQLITE_OK)
		DP_LOGE("sqlite3_bind_int is failed.");
	ret = sqlite3_step(stmt);
	if (ret != SQLITE_OK && ret != SQLITE_DONE)
		FINALIZE_ON_ERROR(stmt);

	if (sqlite3_finalize(stmt) != SQLITE_OK)
		DP_LOGE("sqlite3_finalize is failed.");
	close();
	return true;
}

bool DownloadHistoryDB::deleteMultipleItem(queue <unsigned int> &q)
{
	int ret = 0;
	unsigned int historyId = -1;
	sqlite3_stmt *stmt = NULL;
	char *errmsg = NULL;

	DP_LOG_FUNC();

	if (!open()) {
		DP_LOGE("historyDB is NULL");
		return false;
	}
	ret = sqlite3_exec(historyDb, "PRAGMA synchronous=OFF;\
		PRAGMA count_changes=OFF; PRAGMA temp_store=memory;",
		NULL, NULL, &errmsg);
	if (SQLITE_OK != ret) {
		sqlite3_free(errmsg);
		close();
		return false;
	}

	DP_LOGD("queue size[%d]",q.size());
	while (!q.empty()) {
		ret = sqlite3_prepare_v2(historyDb, "delete from history where historyid=?",
			-1, &stmt, NULL);
		if (ret != SQLITE_OK)
			FINALIZE_ON_ERROR(stmt);
		historyId = q.front();
		q.pop();
		if (sqlite3_bind_int(stmt, 1, historyId) != SQLITE_OK)
			DP_LOGE("sqlite3_bind_int is failed.");
		ret = sqlite3_step(stmt);
		if (ret != SQLITE_OK && ret != SQLITE_DONE)
			FINALIZE_ON_ERROR(stmt);
	}

	if (sqlite3_finalize(stmt) != SQLITE_OK)
		DP_LOGE("sqlite3_finalize is failed.");
	close();
	return true;
}

bool DownloadHistoryDB::clearData(void)
{
	int ret = 0;
	sqlite3_stmt *stmt = NULL;

	DP_LOG_FUNC();

	if (!open()) {
		DP_LOGE("historyDB is NULL");
		return false;
	}

	ret = sqlite3_prepare_v2(historyDb, "delete from history", -1, &stmt, NULL);
	if (ret != SQLITE_OK)
		FINALIZE_ON_ERROR(stmt);

	ret = sqlite3_step(stmt);
	if (ret != SQLITE_DONE)
		FINALIZE_ON_ERROR(stmt);

	if (sqlite3_finalize(stmt) != SQLITE_OK)
		DP_LOGE("sqlite3_finalize is failed.");
	close();
	return true;
}

