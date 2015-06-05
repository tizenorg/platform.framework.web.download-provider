/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd All Rights Reserved
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

//#include <string.h>
//#include <errno.h>
#include <stdio.h>
#include <stdlib.h> // alloc
//#include <unistd.h> // unlink

#include <sqlite3.h>

#include "download-provider.h"
#include "download-provider-db-defs.h"
#include "download-provider-db.h"
#include "download-provider-log.h"
#include "download-provider-utils.h"


static void __basic_property(sqlite3 *handle)
{
	if (sqlite3_exec(handle, "PRAGMA journal_mode=PERSIST;", 0, 0, 0) != SQLITE_OK)
		TRACE_ERROR("check property journal_mode:PERSIST");
	if (sqlite3_exec(handle, "PRAGMA foreign_keys=ON;", 0, 0, 0) != SQLITE_OK)
		TRACE_ERROR("check property foreign_keys:ON");
}

static void __dp_finalize(sqlite3_stmt *stmt)
{
	if (stmt != 0) {
		if (sqlite3_finalize(stmt) != SQLITE_OK) {
			sqlite3 *handle = sqlite3_db_handle(stmt);
			TRACE_ERROR("sqlite3_finalize:%s", sqlite3_errmsg(handle));
		}
	}
}

static int __check_table(sqlite3 *handle, char *table)
{
	sqlite3_stmt *stmt = NULL;

	if (handle == 0 || table == NULL) {
		TRACE_ERROR("check handle or table:%s", table);
		return -1;
	}

	char *query = sqlite3_mprintf("SELECT name FROM sqlite_master WHERE type='table' AND name='%s'", table);
	if (query == NULL) {
		TRACE_ERROR("failed to make query statement");
		return -1;
	}
	int ret = sqlite3_prepare_v2(handle, query, -1, &stmt, NULL);
	sqlite3_free(query);
	int result = 0;
	if (ret != SQLITE_OK) {
		TRACE_ERROR("sqlite3_prepare:%s", sqlite3_errmsg(handle));
		result = -1;
	}
	if (result == 0 && sqlite3_step(stmt) != SQLITE_ROW) {
		TRACE_DEBUG("not found table:%s", table);
		result = -1;
	}
	__dp_finalize(stmt);
	return result;
}

static int __rebuild_client_tables(sqlite3 *handle)
{
	int ret = SQLITE_OK;

	if (__check_table(handle, DP_TABLE_LOGGING) < 0) {
		ret = sqlite3_exec(handle, DP_SCHEMA_LOGGING, 0, 0, 0);
		if (ret == SQLITE_OK) {
			ret = sqlite3_exec(handle, DP_SCHEMA_LOGGING_INDEX, 0, 0, 0);
		}
	}
	if (ret == SQLITE_OK && __check_table(handle, DP_TABLE_DOWNLOAD) < 0) {
		ret = sqlite3_exec(handle, DP_SCHEMA_DOWNLOAD, 0, 0, 0);
	}
	if (ret == SQLITE_OK && __check_table(handle, DP_TABLE_REQUEST) < 0) {
		ret = sqlite3_exec(handle, DP_SCHEMA_REQUEST, 0, 0, 0);
	}
	if (ret == SQLITE_OK && __check_table(handle, DP_TABLE_HEADERS) < 0) {
		ret = sqlite3_exec(handle, DP_SCHEMA_HEADER, 0, 0, 0);
	}
	if (ret == SQLITE_OK && __check_table(handle, DP_TABLE_NOTIFICATION) < 0) {
		ret = sqlite3_exec(handle, DP_SCHEMA_NOTIFICATION, 0, 0, 0);
	}
	if (ret != SQLITE_OK) {
		TRACE_ERROR("create tables:%d error:%s", ret, sqlite3_errmsg(handle));
		return -1;
	}
	return 0;
}

static int __rebuild_client_manager_tables(sqlite3 *handle)
{
	int ret = SQLITE_OK;
	if (__check_table(handle, DP_TABLE_CLIENTS) < 0) {
		ret = sqlite3_exec(handle, DP_SCHEMA_CLIENTS, 0, 0, 0);
	}
	if (ret != SQLITE_OK) {
		TRACE_ERROR("create tables:%d error:%s", ret, sqlite3_errmsg(handle));
		return -1;
	}
	return 0;
}

static int __db_open(sqlite3 **handle, char *database)
{
	if (sqlite3_open_v2(database, handle, SQLITE_OPEN_READWRITE, NULL) != SQLITE_OK) {
		int errorcode = sqlite3_errcode(*handle);
		TRACE_ERROR("error(%d):%s", errorcode, sqlite3_errmsg(*handle));
		*handle = 0;
		if (errorcode == SQLITE_CORRUPT) { // remove & re-create
			TRACE_SECURE_INFO("unlink [%s]", database);
			unlink(database);
			errorcode = SQLITE_CANTOPEN;
		}
		if (errorcode == SQLITE_CANTOPEN) {
			// create empty database
			if (sqlite3_open(database, handle) != SQLITE_OK ) {
				TRACE_SECURE_INFO("failed to connect:%s", database);
				unlink(database);
				*handle = 0;
				return -1;
			}
		} else {
			TRACE_ERROR("can not handle this error:%d", errorcode);
			*handle = 0;
			return -1;
		}
	}
	__basic_property(*handle);
	return 0;
}

int dp_db_check_connection(void *handle)
{
	if (handle == 0) {
		TRACE_ERROR("connection handler is null");
		return -1;
	}
	int phighwater = 0;
	int pcur = 0;
	int ret = sqlite3_db_status(handle, SQLITE_DBSTATUS_STMT_USED, &pcur, &phighwater, 0);
	if (ret != SQLITE_OK) {
		TRACE_INFO("sql(%p) error:%d, used memory:%d, highwater:%d", handle, ret, pcur, phighwater);
		return -1;
	}
	return 0;
}

int dp_db_open_client_manager(void **handle)
{
	if (*handle == 0) {
		char *database = sqlite3_mprintf("%s/%s", DATABASE_DIR, DP_DBFILE_CLIENTS);
		if (database == NULL) {
			TRACE_ERROR("failed to make clients database file path");
			return -1;
		}
		if (__db_open((sqlite3 **)handle, database) < 0) {
			TRACE_ERROR("failed to open %s", database);
			*handle = 0;
		} else {
			// whenever open new handle, check all tables. it's simple
			if (__rebuild_client_manager_tables(*handle) < 0) {
				dp_db_close(*handle);
				*handle = 0;
			}
		}
		sqlite3_free(database);
	}
	return *handle ? 0 : -1;
}

static char *__dp_db_get_client_db_path(char *pkgname)
{
	if (pkgname == NULL)
		return NULL;
	return sqlite3_mprintf("%s/clients/.%s", DATABASE_DIR, pkgname);
}

// 0 : remove, -1: error or skip by diff_time
int dp_db_remove_database(char *pkgname, long now_time, long diff_time)
{
	// get file name
	char *database = __dp_db_get_client_db_path(pkgname);
	if (database == NULL) {
		TRACE_ERROR("failed to make db file path");
		return -1;
	}
	int result = -1;
	// get modified time of database file.
	long modified_time = dp_get_file_modified_time(database);
	if (modified_time >= now_time) {
		TRACE_ERROR("check system timezone %ld vs %ld", modified_time, now_time);
	} else if ((now_time - modified_time) > diff_time) {
		char *database_journal = sqlite3_mprintf("%s-journal", database);
		if (database_journal == NULL) {
			TRACE_ERROR("failed to make db journal file path");
		} else {
			if (dp_remove_file(database_journal) < 0) {
				TRACE_ERROR("failed to remove db journal file path");
			} else {
				if (dp_remove_file(database) < 0) {
					TRACE_ERROR("failed to remove db file path");
				} else {
					result = 0;
				}
			}
			sqlite3_free(database_journal);
		}
	}
	sqlite3_free(database);
	return result;
}

int dp_db_open_client_v2(void **handle, char *pkgname)
{
	char *database = __dp_db_get_client_db_path(pkgname);
	if (database == NULL) {
		TRACE_ERROR("failed to make db file path");
		return -1;
	}
	if (sqlite3_open_v2(database, (sqlite3 **)handle, SQLITE_OPEN_READWRITE, NULL) != SQLITE_OK) {
		int errorcode = sqlite3_errcode(*handle);
		TRACE_ERROR("error(%d):%s", errorcode, sqlite3_errmsg(*handle));
		*handle = 0;
		sqlite3_free(database);
		return -1;
	}
	sqlite3_free(database);
	__basic_property(*handle);
	return 0;
}

int dp_db_open_client(void **handle, char *pkgname)
{
	if (*handle == 0) {
		char *database = __dp_db_get_client_db_path(pkgname);
		if (database == NULL) {
			TRACE_ERROR("failed to make db file path");
			return -1;
		}
		if (__db_open((sqlite3 **)handle, database) < 0) {
			TRACE_SECURE_ERROR("failed to open %s", database);
			*handle = 0;
		} else {
			// whenever open new handle, check all tables. it's simple
			if (__rebuild_client_tables(*handle) < 0) {
				dp_db_close(*handle);
				*handle = 0;
			}
		}
		sqlite3_free(database);
	}
	return *handle ? 0 : -1;
}

void dp_db_close(void *handle)
{
	if (handle != 0) {
		// remove empty page of db
		//sqlite3_exec(handle, "VACUUM;", 0, 0, 0);
		if (sqlite3_close((sqlite3 *)handle) != SQLITE_OK)
			TRACE_ERROR("check sqlite close");
	}
}

void dp_db_reset(void *stmt)
{
	if (stmt != 0) {
		sqlite3_stmt *stmtp = stmt;
		sqlite3_clear_bindings(stmtp);
		if (sqlite3_reset(stmtp) != SQLITE_OK) {
			sqlite3 *handle = sqlite3_db_handle(stmtp);
			TRACE_ERROR("reset:%s", sqlite3_errmsg(handle));
		}
	}
}

void dp_db_finalize(void *stmt)
{
	__dp_finalize((sqlite3_stmt *)stmt);
}

int dp_db_get_errorcode(void *handle)
{
	if (handle == 0) {
		TRACE_ERROR("check connection handle");
		return DP_ERROR_DISK_BUSY;
	}
	int errorcode = sqlite3_errcode((sqlite3 *)handle);
	if (errorcode == SQLITE_FULL) {
		TRACE_ERROR("SQLITE_FULL-NO_SPACE");
		return DP_ERROR_NO_SPACE;
	} else if (errorcode == SQLITE_TOOBIG ||
			errorcode == SQLITE_LOCKED || errorcode == SQLITE_BUSY) {
		TRACE_ERROR("DISK_BUSY %s", sqlite3_errmsg((sqlite3 *)handle));
		return DP_ERROR_DISK_BUSY;
	}
	return DP_ERROR_NONE;
}


#define DP_DB_PARAM_NULL_CHECK do {\
	if (handle == 0) {\
		TRACE_ERROR("check connection handle");\
		return -1;\
	}\
} while(0)

#define DP_DB_BUFFER_NULL_CHECK(buffer) do {\
	if (buffer == NULL) {\
		TRACE_ERROR("check available memory");\
		return -1;\
	}\
} while(0)

#define DP_DB_BASIC_EXCEPTION_CHECK do {\
	if (errorcode != SQLITE_OK) {\
		if ((*error = dp_db_get_errorcode(handle)) == DP_ERROR_NONE)\
			*error = DP_ERROR_INVALID_PARAMETER;\
		__dp_finalize(stmt);\
		return -1;\
	}\
} while(0)

#define DP_DB_WRITE_STEP_EXCEPTION_CHECK do {\
	errorcode = sqlite3_step(stmt);\
	__dp_finalize(stmt);\
	if (errorcode != SQLITE_DONE) {\
		if ((*error = dp_db_get_errorcode(handle)) == DP_ERROR_NONE)\
			*error = DP_ERROR_INVALID_PARAMETER;\
		return -1;\
	}\
} while(0)

int dp_db_get_ids(void *handle, const char *table, char *idcolumn, int *ids, const char *where, const int limit, char *ordercolumn, char *ordering, int *error)
{
	*error = DP_ERROR_INVALID_PARAMETER;
	DP_DB_PARAM_NULL_CHECK;
	int errorcode = SQLITE_OK;
	int rows_count = 0;
	sqlite3_stmt *stmt = NULL;

	char *order_query = NULL;
	if (ordercolumn != NULL)
		order_query = sqlite3_mprintf("ORDER BY %s %s", ordercolumn, ( ordering == NULL ? "ASC" : ordering ));

	if (idcolumn == NULL)
		idcolumn = DP_DB_COL_ID;

	char *query = sqlite3_mprintf("SELECT %s FROM %s %s %s LIMIT ?", idcolumn, table, ( where == NULL ? "" : where ), ( order_query == NULL ? "" : order_query ));
	DP_DB_BUFFER_NULL_CHECK(query);
	errorcode = sqlite3_prepare_v2(handle, query, -1, &stmt, NULL);
	//TRACE_DEBUG("debug query:%s", query);
	sqlite3_free(query);
	if (order_query != NULL)
		sqlite3_free(order_query);
	DP_DB_BASIC_EXCEPTION_CHECK;

	errorcode = sqlite3_bind_int(stmt, 1, limit);
	DP_DB_BASIC_EXCEPTION_CHECK;

	*error = DP_ERROR_NONE;
	while ((errorcode = sqlite3_step(stmt)) == SQLITE_ROW) {
		if (sqlite3_column_type(stmt, 0) == SQLITE_INTEGER) {
			int columnvalue = sqlite3_column_int(stmt, 0);
			//TRACE_DEBUG("id(%d):%d", rows_count, columnvalue);
			ids[rows_count++] = columnvalue;
		}
	}
	__dp_finalize(stmt);
	return rows_count;
}

int dp_db_get_crashed_ids(void *handle, const char *table, int *ids, const int limit, int *error)
{
	// make where.
	//get ids if state is QUEUED, CONNECTING or DOWNLOADING with auto_download
	char *where = sqlite3_mprintf("WHERE %s IS 1 AND (%s IS %d OR %s IS %d OR %s IS %d)",
	DP_DB_COL_AUTO_DOWNLOAD,
	DP_DB_COL_STATE, DP_STATE_DOWNLOADING,
	DP_DB_COL_STATE, DP_STATE_CONNECTING,
	DP_DB_COL_STATE, DP_STATE_QUEUED);
	if (where != NULL) {
		int rows_count = dp_db_get_ids(handle, table, DP_DB_COL_ID, ids, where, limit, NULL, NULL, error);
		sqlite3_free(where);
		return rows_count;
	}
	*error = DP_ERROR_OUT_OF_MEMORY;
	return -1;
}


int dp_db_check_duplicated_int(void *handle, const char *table, const char *column, const int value, int *error)
{
	*error = DP_ERROR_INVALID_PARAMETER;
	DP_DB_PARAM_NULL_CHECK;
	int errorcode = SQLITE_OK;
	int count = 0;
	sqlite3_stmt *stmt = NULL;
	char *query = sqlite3_mprintf("SELECT count(*) FROM %s WHERE %s IS ?", table, column);
	DP_DB_BUFFER_NULL_CHECK(query);
	//TRACE_DEBUG("debug query:%s", query);
	errorcode = sqlite3_prepare_v2(handle, query, -1, &stmt, NULL);
	sqlite3_free(query);
	DP_DB_BASIC_EXCEPTION_CHECK;

	errorcode = sqlite3_bind_int(stmt, 1, value);
	DP_DB_BASIC_EXCEPTION_CHECK;

	*error = DP_ERROR_NONE;
	errorcode = sqlite3_step(stmt);
	if (errorcode == SQLITE_ROW) {
		count = sqlite3_column_int(stmt, 0);
	} else {
		count = 0;
	}
	__dp_finalize(stmt);
	return count;
}

int dp_db_check_duplicated_string(void *handle, const int id, const char *table, const char *column, int is_like, const char *value, int *error)
{
	*error = DP_ERROR_INVALID_PARAMETER;
	DP_DB_PARAM_NULL_CHECK;
	int errorcode = SQLITE_OK;
	int count = 0;
	sqlite3_stmt *stmt = NULL;

	char *id_check = NULL;
	if (id >= 0) {
		id_check = sqlite3_mprintf("AND %s IS ?", DP_DB_COL_ID);
	}
	char *query = NULL;
	if (is_like > 0)
		query = sqlite3_mprintf("SELECT count(*) FROM %s WHERE %s LIKE ? %s", table, column, (id_check == NULL ? "" : id_check));
	else
		query = sqlite3_mprintf("SELECT count(*) FROM %s WHERE %s %s ? %s", table, column, (is_like == 0 ? "IS" : "IS NOT"), (id_check == NULL ? "" : id_check));
	if (id_check != NULL)
		sqlite3_free(id_check);
	DP_DB_BUFFER_NULL_CHECK(query);
	//TRACE_DEBUG("debug query:%s", query);
	errorcode = sqlite3_prepare_v2(handle, query, -1, &stmt, NULL);
	sqlite3_free(query);
	DP_DB_BASIC_EXCEPTION_CHECK;

	errorcode = sqlite3_bind_text(stmt, 1, value, -1, SQLITE_STATIC);
	DP_DB_BASIC_EXCEPTION_CHECK;
	if (id >= 0) {
		errorcode = sqlite3_bind_int(stmt, 2, id);
		DP_DB_BASIC_EXCEPTION_CHECK;
	}

	*error = DP_ERROR_NONE;
	errorcode = sqlite3_step(stmt);
	if (errorcode == SQLITE_ROW) {
		count = sqlite3_column_int(stmt, 0);
	} else {
		count = 0;
	}
	__dp_finalize(stmt);
	return count;
}

int dp_db_update_client_info(void *handle, const char *pkgname, const char *smack, const int uid, const int gid, int *error)
{
	*error = DP_ERROR_INVALID_PARAMETER;
	DP_DB_PARAM_NULL_CHECK;
	if (pkgname == NULL) {
		TRACE_ERROR("check pkgname");
		return -1;
	}

	int is_update = dp_db_check_duplicated_string(handle, -1, DP_TABLE_CLIENTS, DP_DB_COL_PACKAGE, 0, pkgname, error);
	if (is_update < 0) {
		if ((*error = dp_db_get_errorcode(handle)) == DP_ERROR_NONE)
			*error = DP_ERROR_DISK_BUSY;
		return -1;
	}

	int errorcode = SQLITE_OK;
	sqlite3_stmt *stmt = NULL;
	char *query = NULL;

	if (is_update == 0)
		query = sqlite3_mprintf(
			"INSERT INTO %s (%s, %s, %s, %s, %s, %s, %s) VALUES (?, ?, %d, %d, 0, DATETIME('NOW'), DATETIME('NOW'))",
				DP_TABLE_CLIENTS, DP_DB_COL_SMACK_LABEL, DP_DB_COL_PACKAGE, DP_DB_COL_UID,
				DP_DB_COL_GID, DP_DB_COL_REQUEST_COUNT,
				DP_DB_COL_CREATE_TIME, DP_DB_COL_ACCESS_TIME, uid, gid);
	else
		query = sqlite3_mprintf("UPDATE %s SET %s = ?, %s = %d, %s = %d, %s = DATETIME('NOW') WHERE %s IS ?",
				DP_TABLE_CLIENTS, DP_DB_COL_SMACK_LABEL, DP_DB_COL_UID,
				uid, DP_DB_COL_GID, gid, DP_DB_COL_ACCESS_TIME, DP_DB_COL_PACKAGE);
	DP_DB_BUFFER_NULL_CHECK(query);
	//TRACE_DEBUG("debug query:%s", query);
	errorcode = sqlite3_prepare_v2(handle, query, -1, &stmt, NULL);
	sqlite3_free(query);
	DP_DB_BASIC_EXCEPTION_CHECK;

	if (smack != NULL) {
		errorcode = sqlite3_bind_text(stmt, 1, smack, -1, SQLITE_STATIC);
		DP_DB_BASIC_EXCEPTION_CHECK;
	}
	errorcode = sqlite3_bind_text(stmt, 2, pkgname, -1, SQLITE_STATIC);
	DP_DB_BASIC_EXCEPTION_CHECK;

	*error = DP_ERROR_NONE;
	DP_DB_WRITE_STEP_EXCEPTION_CHECK;
	return 0;
}

int dp_db_get_client_property_string(void *handle, const char *pkgname, const char *column, unsigned char **value, unsigned *length, int *error)
{
	*error = DP_ERROR_INVALID_PARAMETER;
	DP_DB_PARAM_NULL_CHECK;
	if (pkgname == NULL || column == NULL || value == NULL || length == NULL) {
		TRACE_ERROR("check materials for query");
		return -1;
	}

	int errorcode = SQLITE_OK;
	sqlite3_stmt *stmt = NULL;
	char *query = sqlite3_mprintf("SELECT %s FROM %s WHERE %s IS ? LIMIT 1", column, DP_TABLE_CLIENTS, DP_DB_COL_PACKAGE);
	DP_DB_BUFFER_NULL_CHECK(query);
	//TRACE_DEBUG("debug query:%s", query);
	errorcode = sqlite3_prepare_v2(handle, query, -1, &stmt, NULL);
	sqlite3_free(query);
	DP_DB_BASIC_EXCEPTION_CHECK;

	errorcode = sqlite3_bind_text(stmt, 1, pkgname, -1, SQLITE_STATIC);
	DP_DB_BASIC_EXCEPTION_CHECK;

	*error = DP_ERROR_NONE;
	errorcode = sqlite3_step(stmt);
	*length = 0;
	if (errorcode == SQLITE_ROW) {
		int data_type = sqlite3_column_type(stmt, 0);
		if (data_type == SQLITE_TEXT) {
			int getbytes = sqlite3_column_bytes(stmt, 0);
			if (getbytes > 0) {
				unsigned char *getstr = (unsigned char *)calloc(getbytes + 1, sizeof(unsigned char));
				if (getstr != NULL) {
					memcpy(getstr, sqlite3_column_text(stmt, 0), getbytes * sizeof(unsigned char));
					getstr[getbytes] = '\0';
					*value = getstr;
					*length = getbytes;
				} else {
					TRACE_ERROR("check available system memory");
					*error = DP_ERROR_OUT_OF_MEMORY;
				}
			} else {
				TRACE_DEBUG("no data");
				*error = DP_ERROR_NO_DATA;
			}
		} else {
			TRACE_ERROR("check column type:%d", data_type);
			*error = DP_ERROR_NO_DATA;
		}
	} else if (errorcode == SQLITE_ROW) {
		TRACE_DEBUG("no data");
		*error = DP_ERROR_NO_DATA;
	} else {
		if ((*error = dp_db_get_errorcode(handle)) == DP_ERROR_NONE)
			*error = DP_ERROR_ID_NOT_FOUND;
	}
	__dp_finalize(stmt);
	if (*error != DP_ERROR_NO_DATA && *error != DP_ERROR_NONE)
		return -1;
	return 0;
}

int dp_db_new_logging(void *handle, const int id, const int state, const int errorvalue, int *error)
{
	*error = DP_ERROR_INVALID_PARAMETER;
	DP_DB_PARAM_NULL_CHECK;
	if (id <= 0) {
		TRACE_ERROR("check id:%d", id);
		return -1;
	}
	int errorcode = SQLITE_OK;
	sqlite3_stmt *stmt = NULL;

	char *query = sqlite3_mprintf("INSERT INTO %s (%s, %s, %s, %s, %s) VALUES (?, ?, ?, DATETIME('now'), DATETIME('now'))",
			DP_TABLE_LOGGING, DP_DB_COL_ID, DP_DB_COL_STATE,
			DP_DB_COL_ERRORCODE, DP_DB_COL_CREATE_TIME, DP_DB_COL_ACCESS_TIME);
	DP_DB_BUFFER_NULL_CHECK(query);
	//TRACE_DEBUG("debug query:%s", query);
	errorcode = sqlite3_prepare_v2(handle, query, -1, &stmt, NULL);
	sqlite3_free(query);
	DP_DB_BASIC_EXCEPTION_CHECK;

	errorcode = sqlite3_bind_int(stmt, 1, id);
	DP_DB_BASIC_EXCEPTION_CHECK;
	errorcode = sqlite3_bind_int(stmt, 2, state);
	DP_DB_BASIC_EXCEPTION_CHECK;
	errorcode = sqlite3_bind_int(stmt, 3, errorvalue);
	DP_DB_BASIC_EXCEPTION_CHECK;

	*error = DP_ERROR_NONE;
	DP_DB_WRITE_STEP_EXCEPTION_CHECK;
	return 0;
}

int dp_db_update_logging(void *handle, const int id, const int state, const int errorvalue, int *error)
{
	*error = DP_ERROR_INVALID_PARAMETER;
	DP_DB_PARAM_NULL_CHECK;
	if (id <= 0) {
		TRACE_ERROR("check id:%d", id);
		return -1;
	}
	int errorcode = SQLITE_OK;
	sqlite3_stmt *stmt = NULL;

	char *query = sqlite3_mprintf("UPDATE %s SET %s = ?, %s = ?, %s = DATETIME('now') WHERE %s = ?",
			DP_TABLE_LOGGING, DP_DB_COL_STATE, DP_DB_COL_ERRORCODE,
			DP_DB_COL_ACCESS_TIME, DP_DB_COL_ID);
	DP_DB_BUFFER_NULL_CHECK(query);
	//TRACE_DEBUG("debug query:%s", query);
	errorcode = sqlite3_prepare_v2(handle, query, -1, &stmt, NULL);
	sqlite3_free(query);
	DP_DB_BASIC_EXCEPTION_CHECK;

	errorcode = sqlite3_bind_int(stmt, 1, state);
	DP_DB_BASIC_EXCEPTION_CHECK;
	errorcode = sqlite3_bind_int(stmt, 2, errorvalue);
	DP_DB_BASIC_EXCEPTION_CHECK;
	errorcode = sqlite3_bind_int(stmt, 3, id);
	DP_DB_BASIC_EXCEPTION_CHECK;

	*error = DP_ERROR_NONE;
	DP_DB_WRITE_STEP_EXCEPTION_CHECK;
	return 0;
}

// 0:integer, 1:bigint, 2:string, 3:blob
int dp_db_replace_property(void *handle, const int id, const char *table, const char *column, const void *value, const unsigned length, const unsigned valuetype, int *error)
{
	*error = DP_ERROR_INVALID_PARAMETER;
	DP_DB_PARAM_NULL_CHECK;
	if (id <= 0) {
		TRACE_ERROR("check id:%d", id);
		return -1;
	}
	if (table == NULL || column == NULL || value == NULL) {
		TRACE_ERROR("check materials for query id:%d", id);
		return -1;
	}

	int is_update = dp_db_check_duplicated_int(handle, table, DP_DB_COL_ID, id, error);
	if (is_update < 0) {
		if ((*error = dp_db_get_errorcode(handle)) == DP_ERROR_NONE)
			*error = DP_ERROR_DISK_BUSY;
		return -1;
	}

	int errorcode = SQLITE_OK;
	sqlite3_stmt *stmt = NULL;
	char *query = NULL;

	if (is_update == 0)
		query = sqlite3_mprintf("INSERT INTO %s (%s, %s) VALUES (?, ?)", table, column, DP_DB_COL_ID);
	else
		query = sqlite3_mprintf("UPDATE %s SET %s = ? WHERE %s IS ?", table, column, DP_DB_COL_ID);
	DP_DB_BUFFER_NULL_CHECK(query);
	//TRACE_DEBUG("debug query:%s", query);
	errorcode = sqlite3_prepare_v2(handle, query, -1, &stmt, NULL);
	sqlite3_free(query);
	DP_DB_BASIC_EXCEPTION_CHECK;

	if (valuetype == 0) {
		int *cast_value = (int *)value;
		errorcode = sqlite3_bind_int(stmt, 1, *cast_value);
	} else if (valuetype == 1) {
		sqlite_int64 *cast_value = (sqlite_int64 *)value;
		errorcode = sqlite3_bind_int64(stmt, 1, *cast_value);
	} else if (valuetype == 2) {
		errorcode = sqlite3_bind_text(stmt, 1, (char *)value, -1, SQLITE_STATIC);
	} else if (valuetype == 3) {
		errorcode = sqlite3_bind_blob(stmt, 1, value, (int)length, NULL);
	} else {
		TRACE_ERROR("invalid type:%d", valuetype);
		__dp_finalize(stmt);
		return -1;
	}
	DP_DB_BASIC_EXCEPTION_CHECK;
	errorcode = sqlite3_bind_int(stmt, 2, id);
	DP_DB_BASIC_EXCEPTION_CHECK;

	*error = DP_ERROR_NONE;
	DP_DB_WRITE_STEP_EXCEPTION_CHECK;
	return 0;
}

int dp_db_get_property_string(void *handle, const int id, const char *table, const char *column, unsigned char **value, unsigned *length, int *error)
{
	*error = DP_ERROR_INVALID_PARAMETER;
	DP_DB_PARAM_NULL_CHECK;
	if (id <= 0) {
		TRACE_ERROR("check id:%d", id);
		return -1;
	}
	if (table == NULL || column == NULL || value == NULL || length == NULL) {
		TRACE_ERROR("check materials for query id:%d", id);
		return -1;
	}

	int errorcode = SQLITE_OK;
	sqlite3_stmt *stmt = NULL;
	char *query = sqlite3_mprintf("SELECT %s FROM %s WHERE %s = ? LIMIT 1", column, table, DP_DB_COL_ID);
	DP_DB_BUFFER_NULL_CHECK(query);
	//TRACE_DEBUG("debug query:%s", query);
	errorcode = sqlite3_prepare_v2(handle, query, -1, &stmt, NULL);
	sqlite3_free(query);
	DP_DB_BASIC_EXCEPTION_CHECK;

	errorcode = sqlite3_bind_int(stmt, 1, id);
	DP_DB_BASIC_EXCEPTION_CHECK;

	*error = DP_ERROR_NONE;
	errorcode = sqlite3_step(stmt);
	*length = 0;
	if (errorcode == SQLITE_ROW) {
		int data_type = sqlite3_column_type(stmt, 0);
		if (data_type == SQLITE_TEXT) {
			int getbytes = sqlite3_column_bytes(stmt, 0);
			if (getbytes > 0) {
				unsigned char *getstr = (unsigned char *)calloc(getbytes + 1, sizeof(unsigned char));
				if (getstr != NULL) {
					memcpy(getstr, sqlite3_column_text(stmt, 0), getbytes * sizeof(unsigned char));
					getstr[getbytes] = '\0';
					*value = getstr;
					*length = getbytes;
				} else {
					TRACE_ERROR("check available system memory");
					*error = DP_ERROR_OUT_OF_MEMORY;
				}
			} else {
				TRACE_DEBUG("no data");
				*error = DP_ERROR_NO_DATA;
			}
		} else if (data_type == SQLITE_BLOB) {
			int getbytes = sqlite3_column_bytes(stmt, 0);
			if (getbytes > 0) {
				unsigned char *getstr = (unsigned char *)calloc(getbytes, sizeof(unsigned char));
				if (getstr != NULL) {
					memcpy(getstr, sqlite3_column_blob(stmt, 0), getbytes * sizeof(unsigned char));
					*value = getstr;
					*length = getbytes;
				} else {
					TRACE_ERROR("check available system memory");
					*error = DP_ERROR_OUT_OF_MEMORY;
				}
			}else {
				TRACE_DEBUG("no data");
				*error = DP_ERROR_NO_DATA;
			}
		} else {
			//TRACE_ERROR("check column type:%d", data_type);
			*error = DP_ERROR_NO_DATA;
		}
	} else if (errorcode == SQLITE_ROW) {
		TRACE_DEBUG("no data");
		*error = DP_ERROR_NO_DATA;
	} else {
		if ((*error = dp_db_get_errorcode(handle)) == DP_ERROR_NONE)
			*error = DP_ERROR_ID_NOT_FOUND;
	}
	__dp_finalize(stmt);
	if (*error != DP_ERROR_NO_DATA && *error != DP_ERROR_NONE)
		return -1;
	return 0;
}

int dp_db_get_property_int(void *handle, const int id, const char *table, const char *column, void *value, int *error)
{
	*error = DP_ERROR_INVALID_PARAMETER;
	DP_DB_PARAM_NULL_CHECK;
	if (id <= 0) {
		TRACE_ERROR("check id:%d", id);
		return -1;
	}
	if (table == NULL || column == NULL || value == NULL) {
		TRACE_ERROR("check materials for query id:%d", id);
		return -1;
	}

	int errorcode = SQLITE_OK;
	sqlite3_stmt *stmt = NULL;
	char *query = sqlite3_mprintf("SELECT %s FROM %s WHERE %s = ? LIMIT 1", column, table, DP_DB_COL_ID);
	DP_DB_BUFFER_NULL_CHECK(query);
	//TRACE_DEBUG("debug query:%s", query);
	errorcode = sqlite3_prepare_v2(handle, query, -1, &stmt, NULL);
	sqlite3_free(query);
	DP_DB_BASIC_EXCEPTION_CHECK;

	errorcode = sqlite3_bind_int(stmt, 1, id);
	DP_DB_BASIC_EXCEPTION_CHECK;

	*error = DP_ERROR_NONE;
	errorcode = sqlite3_step(stmt);
	if (errorcode == SQLITE_ROW) {
		int data_type = sqlite3_column_type(stmt, 0);
		if (data_type == SQLITE_INTEGER) {
			int recv_int = sqlite3_column_int(stmt, 0);
			int *pvalue = value;
			*pvalue = recv_int;
		} else if (data_type == SQLITE_FLOAT) {
			unsigned long long recv_int = sqlite3_column_int64(stmt, 0);
			unsigned long long *pvalue = value;
			*pvalue = recv_int;
		} else {
			TRACE_ERROR("check column type:%d", data_type);
			*error = DP_ERROR_NO_DATA;
		}
	} else if (errorcode == SQLITE_DONE) {
		TRACE_DEBUG("no data");
		*error = DP_ERROR_NO_DATA;
	} else {
		if ((*error = dp_db_get_errorcode(handle)) == DP_ERROR_NONE)
			*error = DP_ERROR_ID_NOT_FOUND;
	}
	__dp_finalize(stmt);
	if (*error != DP_ERROR_NO_DATA && *error != DP_ERROR_NONE)
		return -1;
	return 0;
}

int dp_db_unset_property_string(void *handle, const int id, const char *table, const char *column, int *error)
{
	*error = DP_ERROR_INVALID_PARAMETER;
	DP_DB_PARAM_NULL_CHECK;
	if (id <= 0) {
		TRACE_ERROR("check id:%d", id);
		return -1;
	}
	if (table == NULL || column == NULL) {
		TRACE_ERROR("check materials for query id:%d", id);
		return -1;
	}

	int is_update = dp_db_check_duplicated_int(handle, table, DP_DB_COL_ID, id, error);
	if (is_update < 0) {
		if ((*error = dp_db_get_errorcode(handle)) == DP_ERROR_NONE)
			*error = DP_ERROR_DISK_BUSY;
		return -1;
	} else if (is_update == 0) {
		*error = DP_ERROR_ID_NOT_FOUND;
		return -1;
	}

	int errorcode = SQLITE_OK;
	sqlite3_stmt *stmt = NULL;
	char *query = sqlite3_mprintf("UPDATE %s SET %s = NULL WHERE %s IS ?", table, column, DP_DB_COL_ID);

	DP_DB_BUFFER_NULL_CHECK(query);
	//TRACE_DEBUG("debug query:%s", query);
	errorcode = sqlite3_prepare_v2(handle, query, -1, &stmt, NULL);
	sqlite3_free(query);
	DP_DB_BASIC_EXCEPTION_CHECK;
	errorcode = sqlite3_bind_int(stmt, 1, id);
	DP_DB_BASIC_EXCEPTION_CHECK;

	*error = DP_ERROR_NONE;
	DP_DB_WRITE_STEP_EXCEPTION_CHECK;
	return 0;
}

// "DELETE FROM %s WHERE %s NOT IN (SELECT %s FROM %s ORDER BY %s %s LIMIT %d)"
int dp_db_delete(void *handle, const int id, const char *table, int *error)
{
	*error = DP_ERROR_INVALID_PARAMETER;
	DP_DB_PARAM_NULL_CHECK;
	if (id <= 0) {
		TRACE_ERROR("check id:%d", id);
		return -1;
	}
	if (table == NULL) {
		TRACE_ERROR("check materials for query id:%d", id);
		return -1;
	}

	int is_update = dp_db_check_duplicated_int(handle, table, DP_DB_COL_ID, id, error);
	if (is_update < 0) {
		if ((*error = dp_db_get_errorcode(handle)) == DP_ERROR_NONE)
			*error = DP_ERROR_DISK_BUSY;
		return -1;
	} else if (is_update == 0) {
		*error = DP_ERROR_ID_NOT_FOUND;
		return -1;
	}

	int errorcode = SQLITE_OK;
	sqlite3_stmt *stmt = NULL;
	char *query = sqlite3_mprintf("DELETE FROM %s WHERE %s IS ?", table, DP_DB_COL_ID);

	DP_DB_BUFFER_NULL_CHECK(query);
	//TRACE_DEBUG("debug query:%s", query);
	errorcode = sqlite3_prepare_v2(handle, query, -1, &stmt, NULL);
	sqlite3_free(query);
	DP_DB_BASIC_EXCEPTION_CHECK;
	errorcode = sqlite3_bind_int(stmt, 1, id);
	DP_DB_BASIC_EXCEPTION_CHECK;

	*error = DP_ERROR_NONE;
	DP_DB_WRITE_STEP_EXCEPTION_CHECK;
	return 0;
}

int dp_db_new_header(void *handle, const int id, const char *field, const char *value, int *error)
{
	*error = DP_ERROR_INVALID_PARAMETER;
	DP_DB_PARAM_NULL_CHECK;
	if (id <= 0) {
		TRACE_ERROR("check id:%d", id);
		return -1;
	}
	if (field == NULL) {
		TRACE_ERROR("check field:%s", field);
		return -1;
	}
	int errorcode = SQLITE_OK;
	sqlite3_stmt *stmt = NULL;

	char *query = sqlite3_mprintf("INSERT INTO %s (%s, %s, %s) VALUES (?, ?, ?)",
			DP_TABLE_HEADERS, DP_DB_COL_ID, DP_DB_COL_HEADER_FIELD,
			DP_DB_COL_HEADER_DATA);
	DP_DB_BUFFER_NULL_CHECK(query);
	//TRACE_DEBUG("debug query:%s", query);
	errorcode = sqlite3_prepare_v2(handle, query, -1, &stmt, NULL);
	sqlite3_free(query);
	DP_DB_BASIC_EXCEPTION_CHECK;

	errorcode = sqlite3_bind_int(stmt, 1, id);
	DP_DB_BASIC_EXCEPTION_CHECK;
	errorcode = sqlite3_bind_text(stmt, 2, (char *)field, -1, SQLITE_STATIC);
	DP_DB_BASIC_EXCEPTION_CHECK;
	errorcode = sqlite3_bind_text(stmt, 3, (char *)value, -1, SQLITE_STATIC);
	DP_DB_BASIC_EXCEPTION_CHECK;

	*error = DP_ERROR_NONE;
	DP_DB_WRITE_STEP_EXCEPTION_CHECK;
	return 0;
}

int dp_db_update_header(void *handle, const int id, const char *field, const char *value, int *error)
{
	*error = DP_ERROR_INVALID_PARAMETER;
	DP_DB_PARAM_NULL_CHECK;
	if (id <= 0) {
		TRACE_ERROR("check id:%d", id);
		return -1;
	}
	int errorcode = SQLITE_OK;
	sqlite3_stmt *stmt = NULL;

	char *query = sqlite3_mprintf("UPDATE %s SET %s = ? WHERE %s IS ? AND %s IS ?",
			DP_TABLE_HEADERS, DP_DB_COL_HEADER_DATA,
			DP_DB_COL_ID, DP_DB_COL_HEADER_FIELD);
	DP_DB_BUFFER_NULL_CHECK(query);
	//TRACE_DEBUG("debug query:%s", query);
	errorcode = sqlite3_prepare_v2(handle, query, -1, &stmt, NULL);
	sqlite3_free(query);
	DP_DB_BASIC_EXCEPTION_CHECK;

	errorcode = sqlite3_bind_text(stmt, 1, (char *)value, -1, SQLITE_STATIC);
	DP_DB_BASIC_EXCEPTION_CHECK;
	errorcode = sqlite3_bind_int(stmt, 2, id);
	DP_DB_BASIC_EXCEPTION_CHECK;
	errorcode = sqlite3_bind_text(stmt, 2, (char *)field, -1, SQLITE_STATIC);
	DP_DB_BASIC_EXCEPTION_CHECK;

	*error = DP_ERROR_NONE;
	DP_DB_WRITE_STEP_EXCEPTION_CHECK;
	return 0;
}

int dp_db_get_header_value(void *handle, const int id, const char *field, unsigned char **value, unsigned *length, int *error)
{
	*error = DP_ERROR_INVALID_PARAMETER;
	DP_DB_PARAM_NULL_CHECK;
	if (id <= 0) {
		TRACE_ERROR("check id:%d", id);
		return -1;
	}
	if (field == NULL || value == NULL || length == NULL) {
		TRACE_ERROR("check materials for query id:%d", id);
		return -1;
	}

	int errorcode = SQLITE_OK;
	sqlite3_stmt *stmt = NULL;
	char *query = sqlite3_mprintf("SELECT %s FROM %s WHERE %s IS ? AND %s IS ? LIMIT 1", DP_DB_COL_HEADER_DATA, DP_TABLE_HEADERS, DP_DB_COL_ID, DP_DB_COL_HEADER_FIELD);
	DP_DB_BUFFER_NULL_CHECK(query);
	//TRACE_DEBUG("debug query:%s", query);
	errorcode = sqlite3_prepare_v2(handle, query, -1, &stmt, NULL);
	sqlite3_free(query);
	DP_DB_BASIC_EXCEPTION_CHECK;

	errorcode = sqlite3_bind_int(stmt, 1, id);
	DP_DB_BASIC_EXCEPTION_CHECK;
	errorcode = sqlite3_bind_text(stmt, 2, (char *)field, -1, SQLITE_STATIC);
	DP_DB_BASIC_EXCEPTION_CHECK;

	*error = DP_ERROR_NONE;
	errorcode = sqlite3_step(stmt);
	*length = 0;
	if (errorcode == SQLITE_ROW) {
		int data_type = sqlite3_column_type(stmt, 0);
		if (data_type == SQLITE_TEXT) {
			int getbytes = sqlite3_column_bytes(stmt, 0);
			if (getbytes > 0) {
				unsigned char *getstr = (unsigned char *)calloc(getbytes + 1, sizeof(unsigned char));
				if (getstr != NULL) {
					memcpy(getstr, sqlite3_column_text(stmt, 0), getbytes * sizeof(unsigned char));
					getstr[getbytes] = '\0';
					*value = getstr;
					*length = getbytes;
				} else {
					TRACE_ERROR("check available system memory");
					*error = DP_ERROR_OUT_OF_MEMORY;
				}
			} else {
				TRACE_DEBUG("no data");
				*error = DP_ERROR_NO_DATA;
			}
		} else {
			TRACE_ERROR("check column type:%d", data_type);
			*error = DP_ERROR_NO_DATA;
		}
	} else if (errorcode == SQLITE_ROW) {
		TRACE_DEBUG("no data");
		*error = DP_ERROR_NO_DATA;
	} else {
		if ((*error = dp_db_get_errorcode(handle)) == DP_ERROR_NONE)
			*error = DP_ERROR_ID_NOT_FOUND;
	}
	__dp_finalize(stmt);
	if (*error != DP_ERROR_NO_DATA && *error != DP_ERROR_NONE)
		return -1;
	return 0;
}

// not supprot blob as column & value for additional condition
int dp_db_cond_delete(void *handle, const int id, const char *table, const char *column, const void *value, const unsigned valuetype, int *error)
{
	*error = DP_ERROR_INVALID_PARAMETER;
	DP_DB_PARAM_NULL_CHECK;
	if (id <= 0) {
		TRACE_ERROR("check id:%d", id);
		return -1;
	}
	if (table == NULL || column == NULL || value == NULL) {
		TRACE_ERROR("check materials for query id:%d", id);
		return -1;
	}

	int errorcode = SQLITE_OK;
	sqlite3_stmt *stmt = NULL;
	char *query = sqlite3_mprintf("DELETE FROM %s WHERE %s IS ? AND %s IS ?", table, DP_DB_COL_ID, column);

	DP_DB_BUFFER_NULL_CHECK(query);
	//TRACE_DEBUG("debug query:%s", query);
	errorcode = sqlite3_prepare_v2(handle, query, -1, &stmt, NULL);
	sqlite3_free(query);
	DP_DB_BASIC_EXCEPTION_CHECK;
	errorcode = sqlite3_bind_int(stmt, 1, id);
	DP_DB_BASIC_EXCEPTION_CHECK;

	if (valuetype == 0) {
		int *cast_value = (int *)value;
		errorcode = sqlite3_bind_int(stmt, 2, *cast_value);
	} else if (valuetype == 1) {
		sqlite_int64 *cast_value = (sqlite_int64 *)value;
		errorcode = sqlite3_bind_int64(stmt, 2, *cast_value);
	} else if (valuetype == 2) {
		errorcode = sqlite3_bind_text(stmt, 2, (char *)value, -1, SQLITE_STATIC);
	}
	DP_DB_BASIC_EXCEPTION_CHECK;

	*error = DP_ERROR_NONE;
	DP_DB_WRITE_STEP_EXCEPTION_CHECK;
	return 0;
}

int dp_db_get_cond_ids(void *handle, const char *table, const char *getcolumn, const char *column, const int value, int *ids, const int limit, int *error)
{
	*error = DP_ERROR_INVALID_PARAMETER;
	DP_DB_PARAM_NULL_CHECK;
	int errorcode = SQLITE_OK;
	int rows_count = 0;
	sqlite3_stmt *stmt = NULL;
	char *query = sqlite3_mprintf("SELECT %s FROM %s WHERE %s IS ?", getcolumn, table, column);
	DP_DB_BUFFER_NULL_CHECK(query);
	//TRACE_DEBUG("debug query:%s", query);
	errorcode = sqlite3_prepare_v2(handle, query, -1, &stmt, NULL);
	sqlite3_free(query);
	DP_DB_BASIC_EXCEPTION_CHECK;

	errorcode = sqlite3_bind_int(stmt, 1, value);
	DP_DB_BASIC_EXCEPTION_CHECK;

	*error = DP_ERROR_NONE;
	while ((errorcode = sqlite3_step(stmt)) == SQLITE_ROW) {
		if (sqlite3_column_type(stmt, 0) == SQLITE_INTEGER) {
			int columnvalue = sqlite3_column_int(stmt, 0);
			//TRACE_DEBUG("id(%d):%d", rows_count, columnvalue);
			ids[rows_count++] = columnvalue;
		}
	}
	__dp_finalize(stmt);
	return rows_count;
}

int dp_db_get_cond_string(void *handle, const char *table, char *wherecolumn, const int wherevalue, const char *getcolumn, unsigned char **value, unsigned *length, int *error)
{
	*error = DP_ERROR_INVALID_PARAMETER;
	DP_DB_PARAM_NULL_CHECK;
	if (table == NULL || getcolumn == NULL || value == NULL || length == NULL) {
		TRACE_ERROR("check materials for query");
		return -1;
	}

	if (wherecolumn == NULL)
		wherecolumn = DP_DB_COL_ID;

	int errorcode = SQLITE_OK;
	sqlite3_stmt *stmt = NULL;
	char *query = sqlite3_mprintf("SELECT %s FROM %s WHERE %s = ? LIMIT 1", getcolumn, table, wherecolumn);
	DP_DB_BUFFER_NULL_CHECK(query);
	//TRACE_DEBUG("debug query:%s", query);
	errorcode = sqlite3_prepare_v2(handle, query, -1, &stmt, NULL);
	sqlite3_free(query);
	DP_DB_BASIC_EXCEPTION_CHECK;

	errorcode = sqlite3_bind_int(stmt, 1, wherevalue);
	DP_DB_BASIC_EXCEPTION_CHECK;

	*error = DP_ERROR_NONE;
	errorcode = sqlite3_step(stmt);
	*length = 0;
	if (errorcode == SQLITE_ROW) {
		int data_type = sqlite3_column_type(stmt, 0);
		if (data_type == SQLITE_TEXT) {
			int getbytes = sqlite3_column_bytes(stmt, 0);
			if (getbytes > 0) {
				unsigned char *getstr = (unsigned char *)calloc(getbytes + 1, sizeof(unsigned char));
				if (getstr != NULL) {
					memcpy(getstr, sqlite3_column_text(stmt, 0), getbytes * sizeof(unsigned char));
					getstr[getbytes] = '\0';
					*value = getstr;
					*length = getbytes;
				} else {
					TRACE_ERROR("check available system memory");
					*error = DP_ERROR_OUT_OF_MEMORY;
				}
			} else {
				TRACE_DEBUG("no data");
				*error = DP_ERROR_NO_DATA;
			}
		} else if (data_type == SQLITE_BLOB) {
			int getbytes = sqlite3_column_bytes(stmt, 0);
			if (getbytes > 0) {
				unsigned char *getstr = (unsigned char *)calloc(getbytes, sizeof(unsigned char));
				if (getstr != NULL) {
					memcpy(getstr, sqlite3_column_blob(stmt, 0), getbytes * sizeof(unsigned char));
					*value = getstr;
					*length = getbytes;
				} else {
					TRACE_ERROR("check available system memory");
					*error = DP_ERROR_OUT_OF_MEMORY;
				}
			}else {
				TRACE_DEBUG("no data");
				*error = DP_ERROR_NO_DATA;
			}
		} else {
			TRACE_ERROR("check column type:%d", data_type);
			*error = DP_ERROR_NO_DATA;
		}
	} else if (errorcode == SQLITE_ROW) {
		TRACE_DEBUG("no data");
		*error = DP_ERROR_NO_DATA;
	} else {
		if ((*error = dp_db_get_errorcode(handle)) == DP_ERROR_NONE)
			*error = DP_ERROR_ID_NOT_FOUND;
	}
	__dp_finalize(stmt);
	if (*error != DP_ERROR_NO_DATA && *error != DP_ERROR_NONE)
		return -1;
	return 0;
}

int dp_db_limit_rows(void *handle, const char *table, int limit, int *error)
{
	*error = DP_ERROR_INVALID_PARAMETER;
	DP_DB_PARAM_NULL_CHECK;
	if (table == NULL) {
		TRACE_ERROR("check materials for query");
		return -1;
	}
	if (limit < 0) {
		TRACE_ERROR("check limitation:%d", limit);
		return -1;
	}

	int errorcode = SQLITE_OK;
	sqlite3_stmt *stmt = NULL;
	char *query = sqlite3_mprintf("DELETE FROM %s WHERE %s NOT IN (SELECT %s FROM %s ORDER BY %s ASC LIMIT ?)", table, DP_DB_COL_ID, DP_DB_COL_ID, table, DP_DB_COL_CREATE_TIME);
	DP_DB_BUFFER_NULL_CHECK(query);
	//TRACE_DEBUG("debug query:%s", query);
	errorcode = sqlite3_prepare_v2(handle, query, -1, &stmt, NULL);
	sqlite3_free(query);
	DP_DB_BASIC_EXCEPTION_CHECK;

	errorcode = sqlite3_bind_int(stmt, 1, limit);
	DP_DB_BASIC_EXCEPTION_CHECK;

	// apply "ON DELETE CASCADE"

	*error = DP_ERROR_NONE;
	DP_DB_WRITE_STEP_EXCEPTION_CHECK;
	return 0;
}

int dp_db_limit_time(void *handle, const char *table, int hours, int *error)
{
	*error = DP_ERROR_INVALID_PARAMETER;
	DP_DB_PARAM_NULL_CHECK;
	if (table == NULL) {
		TRACE_ERROR("check materials for query");
		return -1;
	}
	if (hours <= 0) {
		TRACE_ERROR("check limit time:%d", hours);
		return -1;
	}

	int errorcode = SQLITE_OK;
	sqlite3_stmt *stmt = NULL;
	char *query = sqlite3_mprintf("DELETE FROM %s WHERE %s < DATETIME('now','-%d hours')", table, DP_DB_COL_CREATE_TIME, hours);
	DP_DB_BUFFER_NULL_CHECK(query);
	//TRACE_DEBUG("debug query:%s", query);
	errorcode = sqlite3_prepare_v2(handle, query, -1, &stmt, NULL);
	sqlite3_free(query);
	DP_DB_BASIC_EXCEPTION_CHECK;
	*error = DP_ERROR_NONE;
	DP_DB_WRITE_STEP_EXCEPTION_CHECK;
	return 0;
}

int dp_db_get_http_headers_list(void *handle, int id, char **headers, int *error)
{
	int errorcode = SQLITE_OK;
	int headers_index = 0;
	sqlite3_stmt *stmt = NULL;
	*error = DP_ERROR_NONE;
	DP_DB_PARAM_NULL_CHECK;

	if (id <= 0) {
		TRACE_ERROR("[CHECK ID]");
		*error = DP_ERROR_INVALID_PARAMETER;
		return -1;
	}

	errorcode =
		sqlite3_prepare_v2(handle,
			"SELECT header_field, header_data FROM header WHERE id = ?",
			-1, &stmt, NULL);

	DP_DB_BASIC_EXCEPTION_CHECK;

	errorcode = sqlite3_bind_int(stmt, 1, id);

	DP_DB_BASIC_EXCEPTION_CHECK;

	while ((errorcode = sqlite3_step(stmt)) == SQLITE_ROW) {
		int buffer_length = 0;
		char *header_field = (char *)(sqlite3_column_text(stmt, 0));
		char *header_data = (char *)(sqlite3_column_text(stmt, 1));

		// REF : http://www.w3.org/Protocols/rfc2616/rfc2616-sec4.html#sec4.2
		buffer_length = strlen(header_field) + strlen(header_data) + 1;
		char *headers_buffer = calloc(buffer_length + 1, sizeof(char));
		if (headers_buffer == NULL) {
			TRACE_ERROR("[CALLOC] headers_buffer");
			continue;
		}
		int len = snprintf(headers_buffer, buffer_length + 1,
				"%s:%s", header_field, header_data);
		if (len <= 0) {
			if (headers_buffer)
				free(headers_buffer);
				continue;
		} else {
			headers_buffer[len] = '\0';
		}
		headers[headers_index++] = headers_buffer;
	}

	__dp_finalize(stmt);

	return headers_index;
}

