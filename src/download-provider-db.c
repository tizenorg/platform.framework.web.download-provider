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

#include <db-util.h>

#include <string.h>
#include <errno.h>

#include "download-provider-config.h"
#include "download-provider-db.h"
#include "download-provider-slots.h"
#include "download-provider-log.h"
#include "download-provider-pthread.h"

//BASIC
#define DP_DB_BASIC_GET_QUERY_FORMAT "SELECT %s FROM %s WHERE id = ?"
#define DP_DB_BASIC_SET_QUERY_FORMAT "UPDATE %s SET %s = ? WHERE id = ?"
#define DP_DB_BASIC_INSERT_QUERY_FORMAT "INSERT INTO %s (id, %s) VALUES (?, ?)"
#define DP_DB_BASIC_NOW_DATE_QUERY_FORMAT "UPDATE %s SET %s = DATETIME('now') WHERE id = ?"

// COND
#define DP_DB_COND_GET_QUERY_FORMAT "SELECT %s FROM %s WHERE id = ? AND %s = ?"
#define DP_DB_COND_SET_QUERY_FORMAT "UPDATE %s SET %s = ? WHERE id = ? AND %s = ?"

typedef enum {
	DP_DB_QUERY_TYPE_GET = 10,
	DP_DB_QUERY_TYPE_SET = 20,
	DP_DB_QUERY_TYPE_INSERT = 30,
	DP_DB_QUERY_TYPE_NOW_DATE = 40
} db_query_type;

sqlite3 *g_dp_db_handle = 0;

// called when terminating process
void dp_db_close()
{
	if (g_dp_db_handle)
		db_util_close(g_dp_db_handle);
	g_dp_db_handle = 0;
}

// called when launching process or in every API
int dp_db_open()
{
	if (g_dp_db_handle == 0) {
		if (db_util_open(DATABASE_FILE, &g_dp_db_handle,
				DB_UTIL_REGISTER_HOOK_METHOD) != SQLITE_OK) {
			TRACE_ERROR("failed db_util_open [%s][%s]", DATABASE_FILE,
				sqlite3_errmsg(g_dp_db_handle));
			dp_db_close();
			return -1;
		}
		sqlite3_exec(g_dp_db_handle, "PRAGMA journal_mode=PERSIST;", 0, 0, 0);
		sqlite3_exec(g_dp_db_handle, "PRAGMA foreign_keys=ON;", 0, 0, 0);
	}
	return g_dp_db_handle ? 0 : -1;
}

static int __dp_sql_open()
{
	return dp_db_open();
}

static void __dp_finalize(sqlite3_stmt *stmt)
{
	if (sqlite3_finalize(stmt) != SQLITE_OK)
		TRACE_ERROR("failed sqlite3_finalize [%s]",
			sqlite3_errmsg(g_dp_db_handle));
}

int dp_db_get_count_by_limit_time()
{
	int errorcode = SQLITE_OK;
	sqlite3_stmt *stmt = NULL;

	if (__dp_sql_open() < 0) {
		TRACE_ERROR("db_util_open is failed [%s]",
			sqlite3_errmsg(g_dp_db_handle));
		return -1;
	}

	errorcode =
		sqlite3_prepare_v2(g_dp_db_handle,
			"SELECT count(id) FROM logging \
			WHERE createtime < DATETIME('now','-48 hours')",
			-1, &stmt, NULL);
	if (errorcode != SQLITE_OK) {
		TRACE_ERROR("sqlite3_prepare_v2 is failed. [%s]",
			sqlite3_errmsg(g_dp_db_handle));
		__dp_finalize(stmt);
		return -1;
	}
	errorcode = sqlite3_step(stmt);
	if (errorcode == SQLITE_ROW) {
		int count = sqlite3_column_int(stmt, 0);
		__dp_finalize(stmt);
		return count;
	}
	__dp_finalize(stmt);
	return 0;
}

int dp_db_get_list_by_limit_time(dp_request_slots *requests, int limit)
{
	int errorcode = SQLITE_OK;
	int i = 0;
	sqlite3_stmt *stmt = NULL;

	if (__dp_sql_open() < 0) {
		TRACE_ERROR("db_util_open is failed [%s]",
			sqlite3_errmsg(g_dp_db_handle));
		return -1;
	}

	errorcode =
		sqlite3_prepare_v2(g_dp_db_handle,
			"SELECT id, state FROM logging WHERE \
			createtime < DATETIME('now','-48 hours') \
			ORDER BY createtime ASC LIMIT ?",
			-1, &stmt, NULL);
	if (errorcode != SQLITE_OK) {
		TRACE_ERROR("sqlite3_prepare_v2 is failed. [%s]",
			sqlite3_errmsg(g_dp_db_handle));
		__dp_finalize(stmt);
		return -1;
	}
	if (sqlite3_bind_int(stmt, 1, limit)
		!= SQLITE_OK) {
		TRACE_ERROR("sqlite3_bind_int[%s]",
			sqlite3_errmsg(g_dp_db_handle));
		__dp_finalize(stmt);
		return -1;
	}

	while ((errorcode = sqlite3_step(stmt)) == SQLITE_ROW && i < limit) {
		// allocation & initialization
		requests[i].request = dp_request_new();
		// ID
		requests[i].request->id = sqlite3_column_int(stmt, 0);
		// state
		requests[i].request->state = sqlite3_column_int(stmt, 1);

		TRACE_INFO("ID : %d",  requests[i].request->id);
		TRACE_INFO("state : %d",  requests[i].request->state);
		i++;
	}

	__dp_finalize(stmt);
	return i;
}

int dp_db_crashed_list(dp_request_slots *requests, int limit)
{
	int errorcode = SQLITE_OK;
	int i = 0;
	int buffer_length = 0;
	sqlite3_stmt *stmt = NULL;
	char *buffer = NULL;

	if (__dp_sql_open() < 0) {
		TRACE_ERROR("db_util_open is failed [%s]",
			sqlite3_errmsg(g_dp_db_handle));
		return -1;
	}

	errorcode =
		sqlite3_prepare_v2(g_dp_db_handle,
			"SELECT id, state, packagename FROM logging WHERE \
			(state = ? OR state = ? OR state = ?) \
			AND createtime > DATETIME('now','-48 hours') LIMIT ?",
			-1, &stmt, NULL);
	if (errorcode != SQLITE_OK) {
		TRACE_ERROR("sqlite3_prepare_v2 is failed. [%s]",
			sqlite3_errmsg(g_dp_db_handle));
		__dp_finalize(stmt);
		return -1;
	}
	if (sqlite3_bind_int(stmt, 1, DP_STATE_QUEUED) != SQLITE_OK) {
		TRACE_ERROR("sqlite3_bind_int [%s]",
			sqlite3_errmsg(g_dp_db_handle));
		__dp_finalize(stmt);
		return -1;
	}
	if (sqlite3_bind_int(stmt, 2, DP_STATE_DOWNLOADING) != SQLITE_OK) {
		TRACE_ERROR("sqlite3_bind_int [%s]",
			sqlite3_errmsg(g_dp_db_handle));
		__dp_finalize(stmt);
		return -1;
	}
	if (sqlite3_bind_int(stmt, 3, DP_STATE_CONNECTING) != SQLITE_OK) {
		TRACE_ERROR("sqlite3_bind_int [%s]",
			sqlite3_errmsg(g_dp_db_handle));
		__dp_finalize(stmt);
		return -1;
	}

	if (sqlite3_bind_int(stmt, 4, limit)
		!= SQLITE_OK) {
		TRACE_ERROR("sqlite3_bind_int[%s]",
			sqlite3_errmsg(g_dp_db_handle));
		__dp_finalize(stmt);
		return -1;
	}

	while ((errorcode = sqlite3_step(stmt)) == SQLITE_ROW) {
		// allocation & initialization
		requests[i].request = dp_request_new();
		// ID
		requests[i].request->id = sqlite3_column_int(stmt, 0);
		// state
		requests[i].request->state = sqlite3_column_int(stmt, 1);
		// packagename
		buffer = (char *)(sqlite3_column_text(stmt, 2));
		requests[i].request->packagename = NULL;
		if (buffer) {
			buffer_length = strlen(buffer);
			if (buffer_length > 1) {
				requests[i].request->packagename
					= (char *)calloc(buffer_length + 1, sizeof(char));
				memcpy(requests[i].request->packagename, buffer,
					buffer_length * sizeof(char));
				requests[i].request->packagename[buffer_length] = '\0';
			}
		}

		TRACE_INFO("ID : %d",  requests[i].request->id);
		TRACE_INFO("state : %d",  requests[i].request->state);
		TRACE_INFO("packagename : %s",  requests[i].request->packagename);
		i++;
	}

	__dp_finalize(stmt);
	return i;
}

int dp_db_limit_rows(int limit)
{
	int errorcode = SQLITE_OK;
	sqlite3_stmt *stmt = NULL;

	if (limit <= 0) {
		TRACE_ERROR("[CHECK LIMIT] %d", limit);
		return -1;
	}

	if (__dp_sql_open() < 0) {
		TRACE_ERROR("__dp_sql_open[%s]", sqlite3_errmsg(g_dp_db_handle));
		return -1;
	}

	// apply "ON DELETE CASCADE"
	errorcode =
		sqlite3_prepare_v2(g_dp_db_handle,
			"DELETE FROM logging WHERE id NOT IN \
			(SELECT id FROM logging ORDER BY createtime ASC LIMIT ?)",
			-1, &stmt, NULL);
	if (errorcode != SQLITE_OK) {
		TRACE_ERROR("sqlite3_prepare_v2 [%s]",
				sqlite3_errmsg(g_dp_db_handle));
		__dp_finalize(stmt);
		return -1;
	}
	if (sqlite3_bind_int(stmt, 1, limit)
		!= SQLITE_OK) {
		TRACE_ERROR("sqlite3_bind_int[%s]",
			sqlite3_errmsg(g_dp_db_handle));
		__dp_finalize(stmt);
		return -1;
	}
	errorcode = sqlite3_step(stmt);
	if (errorcode == SQLITE_OK || errorcode == SQLITE_DONE) {
		__dp_finalize(stmt);
		return 0;
	}
	__dp_finalize(stmt);
	return -1;
}

dp_request *dp_db_load_logging_request(int id)
{
	int errorcode = SQLITE_OK;
	int buffer_length = 0;
	sqlite3_stmt *stmt = NULL;
	char *buffer = NULL;
	dp_request *request = NULL;

	if (id <= 0) {
		TRACE_ERROR("[CHECK ID]");
		return NULL;
	}

	if (__dp_sql_open() < 0) {
		TRACE_ERROR("db_util_open is failed [%s]",
				sqlite3_errmsg(g_dp_db_handle));
		return NULL;
	}

	errorcode =
		sqlite3_prepare_v2(g_dp_db_handle,
			"SELECT state, errorcode, startcount, packagename \
			FROM logging WHERE id = ?",
			-1, &stmt, NULL);
	if (errorcode != SQLITE_OK) {
		TRACE_ERROR("sqlite3_prepare_v2 is failed. [%s]",
				sqlite3_errmsg(g_dp_db_handle));
		__dp_finalize(stmt);
		return NULL;
	}
	if (sqlite3_bind_int(stmt, 1, id) != SQLITE_OK) {
		TRACE_ERROR("sqlite3_bind_int is failed. [%s]",
				sqlite3_errmsg(g_dp_db_handle));
		__dp_finalize(stmt);
		return NULL;
	}

	if ((errorcode = sqlite3_step(stmt)) == SQLITE_ROW) {
		request = dp_request_new();
		if (request == NULL) {
			TRACE_ERROR("dp_request_new failed");
			__dp_finalize(stmt);
			return NULL;
		}
		request->id = id;
		request->state = sqlite3_column_int(stmt, 0);
		request->error = sqlite3_column_int(stmt, 1);
		request->startcount = sqlite3_column_int(stmt, 2);

		buffer = (char *)(sqlite3_column_text(stmt, 3));
		if (buffer) {
			buffer_length = strlen(buffer);
			if (buffer_length > 1) {
				request->packagename
					= (char *)calloc(buffer_length + 1, sizeof(char));
				memcpy(request->packagename, buffer,
					buffer_length * sizeof(char));
				request->packagename[buffer_length] = '\0';
			}
		}
	} else {
		TRACE_ERROR("sqlite3_step is failed. [%s] errorcode[%d]",
				sqlite3_errmsg(g_dp_db_handle), errorcode);
		__dp_finalize(stmt);
		return NULL;
	}
	__dp_finalize(stmt);
	return request;
}

int dp_db_remove_all(int id)
{
	#if 0
	dp_db_remove(id, DP_DB_TABLE_REQUEST_INFO);
	dp_db_remove(id, DP_DB_TABLE_DOWNLOAD_INFO);
	dp_db_remove(id, DP_DB_TABLE_HTTP_HEADERS);
	dp_db_remove(id, DP_DB_TABLE_NOTIFICATION);
	#endif
	// apply "ON DELETE CASCADE"
	dp_db_remove(id, DP_DB_TABLE_LOG);
	return -1;
}

int dp_db_remove(int id, char *table)
{
	int errorcode = SQLITE_OK;
	int query_len = 0;
	int ret = -1;
	sqlite3_stmt *stmt = NULL;
	char *query_format = NULL;
	char *query = NULL;

	if (__dp_sql_open() < 0) {
		TRACE_ERROR("__dp_sql_open[%s]", sqlite3_errmsg(g_dp_db_handle));
		return -1;
	}

	query_format = "DELETE FROM %s WHERE id = ? ";
	// 2 means the length of one %s
	query_len = strlen(query_format) - 2 + strlen(table);
	if (query_len < strlen(query_format)) {
		TRACE_ERROR("[CHECK QUERY FORMAT] [%s][%s]",
				query_format, table);
		return -1;
	}

	query = (char *)calloc((query_len + 1), sizeof(char));
	if (query == NULL) {
		TRACE_STRERROR("[CALLOC]");
		return -1;
	}
	query[query_len] = '\0';

	ret = snprintf(query, query_len + 1, query_format, table);

	if (ret < 0) {
		TRACE_STRERROR("[CHECK COMBINE] [%s]", query);
		free(query);
		return -1;
	}

	// check error of sqlite3_prepare_v2
	if (sqlite3_prepare_v2
			(g_dp_db_handle, query, -1, &stmt, NULL) != SQLITE_OK) {
		TRACE_ERROR("[PREPARE] [%s]", sqlite3_errmsg(g_dp_db_handle));
		__dp_finalize(stmt);
		free(query);
		return -1;
	}
	free(query);

	if (sqlite3_bind_int(stmt, 1, id)
		!= SQLITE_OK) {
		TRACE_ERROR("sqlite3_bind_int[%s]",
			sqlite3_errmsg(g_dp_db_handle));
		__dp_finalize(stmt);
		return -1;
	}
	errorcode = sqlite3_step(stmt);
	if (errorcode == SQLITE_OK || errorcode == SQLITE_DONE) {
		__dp_finalize(stmt);
		return 0;
	}
	__dp_finalize(stmt);
	return -1;
}

static sqlite3_stmt *__prepare_query(sqlite3 *handle,
		db_query_type type, char *table, char *column)
{
	sqlite3_stmt *stmt = NULL;
	char *query_format = NULL;
	char *query = NULL;
	int ret = -1;

	if (type == DP_DB_QUERY_TYPE_GET) {
		query_format = DP_DB_BASIC_GET_QUERY_FORMAT;
	} else if (type == DP_DB_QUERY_TYPE_SET) {
		query_format = DP_DB_BASIC_SET_QUERY_FORMAT;
	} else if (type == DP_DB_QUERY_TYPE_INSERT) {
		query_format = DP_DB_BASIC_INSERT_QUERY_FORMAT;
	} else if (type == DP_DB_QUERY_TYPE_NOW_DATE) {
		query_format = DP_DB_BASIC_NOW_DATE_QUERY_FORMAT;
	} else {
		TRACE_ERROR("[CHECK QUERY TYPE] [%d]", type);
		return NULL;
	}

	if (type == DP_DB_QUERY_TYPE_GET)
		query = sqlite3_mprintf(query_format, column, table);
	else
		query = sqlite3_mprintf(query_format, table, column);
	if (query == NULL) {
		TRACE_ERROR("[CHECK COMBINE] [%s]", query_format);
		return NULL;
	}

	TRACE_INFO("[QUERY] %s", query);

	ret = sqlite3_prepare_v2(handle, query, -1, &stmt, NULL);
	sqlite3_free(query);
	if ( ret != SQLITE_OK) {
		TRACE_ERROR("[PREPARE] [%s]", sqlite3_errmsg(handle));
		__dp_finalize(stmt);
		return NULL;
	}
	return stmt;
}

int dp_db_insert_column(int id, char *table, char *column,
						db_column_data_type datatype, void *value)
{
	sqlite3_stmt *stmt = NULL;

	if (id <= 0) {
		TRACE_ERROR("[CHECK ID]");
		return -1;
	}

	if (!table) {
		TRACE_ERROR("[CHECK TABLE NAME]");
		return -1;
	}

	if (!column) {
		TRACE_ERROR("[CHECK COLUMN NAME]");
		return -1;
	}

	if (__dp_sql_open() < 0) {
		TRACE_ERROR("[OPEN] [%s]", sqlite3_errmsg(g_dp_db_handle));
		return -1;
	}

	stmt = __prepare_query
			(g_dp_db_handle, DP_DB_QUERY_TYPE_INSERT, table, column);
	if (stmt == NULL) {
		TRACE_ERROR("[PREPARE] [%s]", sqlite3_errmsg(g_dp_db_handle));
		return -1;
	}

	int errorcode = SQLITE_OK;
	if (datatype == DP_DB_COL_TYPE_INT) {
		int *cast_value = value;
		errorcode = sqlite3_bind_int(stmt, 2, *cast_value);
	} else if (datatype == DP_DB_COL_TYPE_INT64) {
#ifdef SQLITE_INT64_TYPE
		sqlite3_int64 *cast_value = value;
		errorcode = sqlite3_bind_int64(stmt, 2, *cast_value);
#else
		int *cast_value = value;
		errorcode = sqlite3_bind_int(stmt, 2, *cast_value);
#endif
	} else if (datatype == DP_DB_COL_TYPE_TEXT) {
		errorcode = sqlite3_bind_text(stmt, 2, (char*)value, -1, NULL);
	} else {
		TRACE_ERROR("[CHECK TYPE] Not Support [%d]", datatype);
		__dp_finalize(stmt);
		return -1;
	}

	if (errorcode != SQLITE_OK) {
		TRACE_ERROR("[BIND] [%d] [%s]",
			datatype, sqlite3_errmsg(g_dp_db_handle));
		__dp_finalize(stmt);
		return -1;
	}

	// VALUES ( id )
	if (sqlite3_bind_int(stmt, 1, id) != SQLITE_OK) {
		TRACE_ERROR("[BIND] [%s]", sqlite3_errmsg(g_dp_db_handle));
		__dp_finalize(stmt);
		return -1;
	}

	errorcode = sqlite3_step(stmt);
	if (errorcode == SQLITE_OK || errorcode == SQLITE_DONE) {
		__dp_finalize(stmt);
		return 0;
	}
	__dp_finalize(stmt);
	return -1;
}

int dp_db_set_column(int id, char *table, char *column,
						db_column_data_type datatype, void *value)
{
	sqlite3_stmt *stmt = NULL;

	if (id <= 0) {
		TRACE_ERROR("[CHECK ID]");
		return -1;
	}

	if (!table) {
		TRACE_ERROR("[CHECK TABLE NAME]");
		return -1;
	}

	if (!column) {
		TRACE_ERROR("[CHECK COLUMN NAME]");
		return -1;
	}

	if (__dp_sql_open() < 0) {
		TRACE_ERROR("[OPEN] [%s]", sqlite3_errmsg(g_dp_db_handle));
		return -1;
	}

	stmt = __prepare_query
			(g_dp_db_handle, DP_DB_QUERY_TYPE_SET, table, column);
	if (stmt == NULL) {
		TRACE_ERROR("[PREPARE] [%s]", sqlite3_errmsg(g_dp_db_handle));
		return -1;
	}

	int errorcode = SQLITE_OK;
	if (datatype == DP_DB_COL_TYPE_INT) {
		int *cast_value = value;
		errorcode = sqlite3_bind_int(stmt, 1, *cast_value);
	} else if (datatype == DP_DB_COL_TYPE_INT64) {
#ifdef SQLITE_INT64_TYPE
		sqlite3_int64 *cast_value = value;
		errorcode = sqlite3_bind_int64(stmt, 1, *cast_value);
#else
		int *cast_value = value;
		errorcode = sqlite3_bind_int(stmt, 1, *cast_value);
#endif
	} else if (datatype == DP_DB_COL_TYPE_TEXT) {
		errorcode = sqlite3_bind_text(stmt, 1, (char*)value, -1, NULL);
	} else {
		TRACE_ERROR("[CHECK TYPE] Not Support [%d]", datatype);
		__dp_finalize(stmt);
		return -1;
	}

	if (errorcode != SQLITE_OK) {
		TRACE_ERROR("[BIND] [%d] [%s]",
			datatype, sqlite3_errmsg(g_dp_db_handle));
		__dp_finalize(stmt);
		return -1;
	}

	// WHERE id = ?
	if (sqlite3_bind_int(stmt, 2, id) != SQLITE_OK) {
		TRACE_ERROR("[BIND] [%s]", sqlite3_errmsg(g_dp_db_handle));
		__dp_finalize(stmt);
		return -1;
	}

	errorcode = sqlite3_step(stmt);
	if (errorcode == SQLITE_OK || errorcode == SQLITE_DONE) {
		__dp_finalize(stmt);
		return 0;
	}
	__dp_finalize(stmt);
	return -1;
}

int dp_db_replace_column(int id, char *table, char *column,
						db_column_data_type datatype, void *value)
{
	if (id <= 0) {
		TRACE_ERROR("[CHECK ID]");
		return -1;
	}

	if (!table) {
		TRACE_ERROR("[CHECK TABLE NAME]");
		return -1;
	}

	if (!column) {
		TRACE_ERROR("[CHECK COLUMN NAME]");
		return -1;
	}

	int check_id = dp_db_get_int_column(id, table, DP_DB_COL_ID);
	if (check_id != id) // INSERT
		return dp_db_insert_column(id, table, column, datatype, value);
	// UPDATE
	return dp_db_set_column(id, table, column, datatype, value);
}

// success : 0
// error   : -1
char *dp_db_get_text_column(int id, char *table, char *column)
{
	sqlite3_stmt *stmt = NULL;

	if (id <= 0) {
		TRACE_ERROR("[CHECK ID]");
		return NULL;
	}

	if (!table) {
		TRACE_ERROR("[CHECK TABLE NAME]");
		return NULL;
	}

	if (!column) {
		TRACE_ERROR("[CHECK COLUMN NAME]");
		return NULL;
	}

	if (__dp_sql_open() < 0) {
		TRACE_ERROR("[OPEN] [%s]", sqlite3_errmsg(g_dp_db_handle));
		return NULL;
	}

	stmt = __prepare_query
			(g_dp_db_handle, DP_DB_QUERY_TYPE_GET, table, column);
	if (stmt == NULL) {
		TRACE_ERROR("[PREPARE] [%s]", sqlite3_errmsg(g_dp_db_handle));
		return NULL;
	}

	// WHERE id = ?
	if (sqlite3_bind_int(stmt, 1, id) != SQLITE_OK) {
		TRACE_ERROR("[BIND] [%s]", sqlite3_errmsg(g_dp_db_handle));
		__dp_finalize(stmt);
		return NULL;
	}

	if (sqlite3_step(stmt) == SQLITE_ROW) {
		int buffer_length = 0;
		char *columntext = NULL;
		char *buffer = (char *)(sqlite3_column_text(stmt, 0));
		if (buffer && (buffer_length = strlen(buffer)) > 1) {
			columntext = (char *)calloc(buffer_length + 1, sizeof(char));
			memcpy(columntext, buffer, buffer_length * sizeof(char));
			columntext[buffer_length] = '\0';
		}
		__dp_finalize(stmt);
		return columntext;
	}
	__dp_finalize(stmt);
	return NULL;
}

int dp_db_get_int_column(int id, char *table, char *column)
{
	sqlite3_stmt *stmt = NULL;

	if (id <= 0) {
		TRACE_ERROR("[CHECK ID]");
		return -1;
	}

	if (!table) {
		TRACE_ERROR("[CHECK TABLE NAME]");
		return -1;
	}

	if (!column) {
		TRACE_ERROR("[CHECK COLUMN NAME]");
		return -1;
	}

	if (__dp_sql_open() < 0) {
		TRACE_ERROR("[OPEN] [%s]", sqlite3_errmsg(g_dp_db_handle));
		return -1;
	}

	stmt = __prepare_query
			(g_dp_db_handle, DP_DB_QUERY_TYPE_GET, table, column);
	if (stmt == NULL) {
		TRACE_ERROR("[PREPARE] [%s]", sqlite3_errmsg(g_dp_db_handle));
		return -1;
	}

	// WHERE id = ?
	if (sqlite3_bind_int(stmt, 1, id) != SQLITE_OK) {
		TRACE_ERROR("[BIND] [%s]", sqlite3_errmsg(g_dp_db_handle));
		__dp_finalize(stmt);
		return -1;
	}

	if (sqlite3_step(stmt) == SQLITE_ROW) {
		int columnvalue = sqlite3_column_int(stmt, 0);
		__dp_finalize(stmt);
		return columnvalue;
	}
	__dp_finalize(stmt);
	return -1;
}

long long dp_db_get_int64_column(int id, char *table, char *column)
{
	sqlite3_stmt *stmt = NULL;

	if (id <= 0) {
		TRACE_ERROR("[CHECK ID]");
		return -1;
	}

	if (!table) {
		TRACE_ERROR("[CHECK TABLE NAME]");
		return -1;
	}

	if (!column) {
		TRACE_ERROR("[CHECK COLUMN NAME]");
		return -1;
	}

	if (__dp_sql_open() < 0) {
		TRACE_ERROR("[OPEN] [%s]", sqlite3_errmsg(g_dp_db_handle));
		return -1;
	}

	stmt = __prepare_query
			(g_dp_db_handle, DP_DB_QUERY_TYPE_GET, table, column);
	if (stmt == NULL) {
		TRACE_ERROR("[PREPARE] [%s]", sqlite3_errmsg(g_dp_db_handle));
		return -1;
	}

	// WHERE id = ?
	if (sqlite3_bind_int(stmt, 1, id) != SQLITE_OK) {
		TRACE_ERROR("[BIND] [%s]", sqlite3_errmsg(g_dp_db_handle));
		__dp_finalize(stmt);
		return -1;
	}

	if (sqlite3_step(stmt) == SQLITE_ROW) {
		long long columnvalue = sqlite3_column_int64(stmt, 0);
		__dp_finalize(stmt);
		return columnvalue;
	}
	__dp_finalize(stmt);
	return -1;
}

int dp_db_update_date(int id, char *table, char *column)
{
	int errorcode = SQLITE_OK;
	sqlite3_stmt *stmt = NULL;

	if (id <= 0) {
		TRACE_ERROR("[CHECK ID]");
		return -1;
	}

	if (__dp_sql_open() < 0) {
		TRACE_ERROR("__dp_sql_open [%s]", sqlite3_errmsg(g_dp_db_handle));
		return -1;
	}

	stmt = __prepare_query
			(g_dp_db_handle, DP_DB_QUERY_TYPE_NOW_DATE, table, column);
	if (stmt == NULL) {
		TRACE_ERROR("[PREPARE] [%s]", sqlite3_errmsg(g_dp_db_handle));
		return -1;
	}

	// WHERE id = ?
	if (sqlite3_bind_int(stmt, 1, id) != SQLITE_OK) {
		TRACE_ERROR("sqlite3_bind_int [%s]",
			sqlite3_errmsg(g_dp_db_handle));
		__dp_finalize(stmt);
		return -1;
	}

	errorcode = sqlite3_step(stmt);
	if (errorcode == SQLITE_OK || errorcode == SQLITE_DONE) {
		__dp_finalize(stmt);
		return 0;
	}
	TRACE_ERROR("Failed : [%s]", sqlite3_errmsg(g_dp_db_handle));
	__dp_finalize(stmt);
	return -1;
}

static sqlite3_stmt *__prepare_cond_query(sqlite3 *handle,
		db_query_type type, char *table,
		char *column, char *cond_column)
{
	sqlite3_stmt *stmt = NULL;
	char *query_format = NULL;
	char *query = NULL;
	int ret = -1;

	if (type == DP_DB_QUERY_TYPE_GET) {
		query_format = DP_DB_COND_GET_QUERY_FORMAT;
	} else if (type == DP_DB_QUERY_TYPE_SET) {
		query_format = DP_DB_COND_SET_QUERY_FORMAT;
	} else {
		TRACE_ERROR("[CHECK QUERY TYPE] [%d]", type);
		return NULL;
	}

	if (type == DP_DB_QUERY_TYPE_GET)
		query = sqlite3_mprintf(query_format, column, table, cond_column);
	else
		query = sqlite3_mprintf(query_format, table, column, cond_column);
	if (query == NULL) {
		TRACE_ERROR("[CHECK COMBINE] [%s]", query_format);
		return NULL;
	}

	TRACE_INFO("[QUERY] %s", query);

	ret = sqlite3_prepare_v2(handle, query, -1, &stmt, NULL);
	sqlite3_free(query);
	if ( ret != SQLITE_OK) {
		TRACE_ERROR("[PREPARE] [%s]", sqlite3_errmsg(handle));
		__dp_finalize(stmt);
		return NULL;
	}
	return stmt;

}

int dp_db_cond_set_column(int id, char *table, char *column,
		db_column_data_type datatype, void *value,
		char *condcolumn, db_column_data_type condtype, void *condvalue)
{
	int errorcode = SQLITE_OK;
	sqlite3_stmt *stmt = NULL;

	if (id <= 0) {
		TRACE_ERROR("[CHECK ID]");
		return -1;
	}

	if (!table) {
		TRACE_ERROR("[CHECK TABLE NAME]");
		return -1;
	}

	if (!column) {
		TRACE_ERROR("[CHECK COLUMN NAME]");
		return -1;
	}

	if (!condcolumn) {
		TRACE_ERROR("[CHECK COLUMN NAME]");
		return -1;
	}

	if (__dp_sql_open() < 0) {
		TRACE_ERROR("[OPEN] [%s]", sqlite3_errmsg(g_dp_db_handle));
		return -1;
	}

	stmt = __prepare_cond_query
			(g_dp_db_handle, DP_DB_QUERY_TYPE_SET, table, column, condcolumn);
	if (stmt == NULL) {
		TRACE_ERROR("[PREPARE] [%s]", sqlite3_errmsg(g_dp_db_handle));
		return -1;
	}

	if (datatype == DP_DB_COL_TYPE_INT) {
		int *cast_value = value;
		errorcode = sqlite3_bind_int(stmt, 1, *cast_value);
	} else if (datatype == DP_DB_COL_TYPE_INT64) {
#ifdef SQLITE_INT64_TYPE
		sqlite3_int64 *cast_value = value;
		errorcode = sqlite3_bind_int64(stmt, 1, *cast_value);
#else
		int *cast_value = value;
		errorcode = sqlite3_bind_int(stmt, 1, *cast_value);
#endif
	} else if (datatype == DP_DB_COL_TYPE_TEXT) {
		errorcode = sqlite3_bind_text(stmt, 1, (char*)value, -1, NULL);
	} else {
		TRACE_ERROR("[CHECK TYPE] Not Support [%d]", datatype);
		__dp_finalize(stmt);
		return -1;
	}
	if (errorcode != SQLITE_OK) {
		TRACE_ERROR("[BIND] [%d] [%s]",
			datatype, sqlite3_errmsg(g_dp_db_handle));
		__dp_finalize(stmt);
		return -1;
	}

	if (condtype == DP_DB_COL_TYPE_INT) {
		int *cast_value = condvalue;
		errorcode = sqlite3_bind_int(stmt, 3, *cast_value);
	} else if (condtype == DP_DB_COL_TYPE_INT64) {
#ifdef SQLITE_INT64_TYPE
		sqlite3_int64 *cast_value = condvalue;
		errorcode = sqlite3_bind_int64(stmt, 3, *cast_value);
#else
		int *cast_value = condvalue;
		errorcode = sqlite3_bind_int(stmt, 3, *cast_value);
#endif
	} else if (condtype == DP_DB_COL_TYPE_TEXT) {
		errorcode = sqlite3_bind_text(stmt, 3, (char*)condvalue, -1, NULL);
	} else {
		TRACE_ERROR("[CHECK TYPE] Not Support [%d]", condtype);
		__dp_finalize(stmt);
		return -1;
	}

	if (errorcode != SQLITE_OK) {
		TRACE_ERROR("[BIND] [%d] [%s]",
			datatype, sqlite3_errmsg(g_dp_db_handle));
		__dp_finalize(stmt);
		return -1;
	}

	// WHERE id = ?
	if (sqlite3_bind_int(stmt, 2, id) != SQLITE_OK) {
		TRACE_ERROR("[BIND] [%s]", sqlite3_errmsg(g_dp_db_handle));
		__dp_finalize(stmt);
		return -1;
	}

	errorcode = sqlite3_step(stmt);
	if (errorcode == SQLITE_OK || errorcode == SQLITE_DONE) {
		__dp_finalize(stmt);
		return 0;
	}
	__dp_finalize(stmt);
	return -1;
}

char *dp_db_cond_get_text_column(int id, char *table, char *column,
						char *condcolumn, db_column_data_type condtype,
						void *condvalue)
{
	sqlite3_stmt *stmt = NULL;

	if (id <= 0) {
		TRACE_ERROR("[CHECK ID]");
		return NULL;
	}

	if (!table) {
		TRACE_ERROR("[CHECK TABLE NAME]");
		return NULL;
	}

	if (!column) {
		TRACE_ERROR("[CHECK COLUMN NAME]");
		return NULL;
	}

	if (__dp_sql_open() < 0) {
		TRACE_ERROR("[OPEN] [%s]", sqlite3_errmsg(g_dp_db_handle));
		return NULL;
	}

	stmt = __prepare_cond_query
			(g_dp_db_handle, DP_DB_QUERY_TYPE_GET, table, column, condcolumn);
	if (stmt == NULL) {
		TRACE_ERROR("[PREPARE] [%s]", sqlite3_errmsg(g_dp_db_handle));
		return NULL;
	}

	int errorcode = SQLITE_OK;
	if (condtype == DP_DB_COL_TYPE_INT) {
		int *cast_value = condvalue;
		errorcode = sqlite3_bind_int(stmt, 2, *cast_value);
	} else if (condtype == DP_DB_COL_TYPE_INT64) {
#ifdef SQLITE_INT64_TYPE
		sqlite3_int64 *cast_value = condvalue;
		errorcode = sqlite3_bind_int64(stmt, 2, *cast_value);
#else
		int *cast_value = condvalue;
		errorcode = sqlite3_bind_int(stmt, 2, *cast_value);
#endif
	} else if (condtype == DP_DB_COL_TYPE_TEXT) {
		errorcode = sqlite3_bind_text(stmt, 2, (char*)condvalue, -1, NULL);
	} else {
		TRACE_ERROR("[CHECK TYPE] Not Support [%d]", condtype);
		__dp_finalize(stmt);
		return NULL;
	}
	if (errorcode != SQLITE_OK) {
		TRACE_ERROR("[BIND] [%d] [%s]",
			condtype, sqlite3_errmsg(g_dp_db_handle));
		__dp_finalize(stmt);
		return NULL;
	}

	// WHERE id = ?
	if (sqlite3_bind_int(stmt, 1, id) != SQLITE_OK) {
		TRACE_ERROR("[BIND] [%s]", sqlite3_errmsg(g_dp_db_handle));
		__dp_finalize(stmt);
		return NULL;
	}

	if (sqlite3_step(stmt) == SQLITE_ROW) {
		int buffer_length = 0;
		char *columntext = NULL;
		char *buffer = (char *)(sqlite3_column_text(stmt, 0));
		if (buffer && (buffer_length = strlen(buffer)) > 1) {
			columntext = (char *)calloc(buffer_length + 1, sizeof(char));
			memcpy(columntext, buffer, buffer_length * sizeof(char));
			columntext[buffer_length] = '\0';
		}
		__dp_finalize(stmt);
		return columntext;
	}
	__dp_finalize(stmt);
	return NULL;
}

int dp_db_cond_remove(int id, char *table,
						char *condcolumn, db_column_data_type condtype,
						void *condvalue)
{
	int errorcode = SQLITE_OK;
	int ret = -1;
	sqlite3_stmt *stmt = NULL;
	char *query_format = NULL;
	char *query = NULL;

	if (id <= 0) {
		TRACE_ERROR("[CHECK ID]");
		return -1;
	}

	if (!table) {
		TRACE_ERROR("[CHECK TABLE NAME]");
		return -1;
	}

	if (!condcolumn) {
		TRACE_ERROR("[CHECK COLUMN NAME]");
		return -1;
	}

	if (__dp_sql_open() < 0) {
		TRACE_ERROR("__dp_sql_open[%s]", sqlite3_errmsg(g_dp_db_handle));
		return -1;
	}

	query_format = "DELETE FROM %s WHERE id = ? AND %s = ?";

	query = sqlite3_mprintf(query_format, table, condcolumn);
	if (query == NULL) {
		TRACE_ERROR("[CHECK COMBINE] [%s]", query_format);
		return -1;
	}

	TRACE_INFO("[QUERY] %s", query);

	ret = sqlite3_prepare_v2(g_dp_db_handle, query, -1, &stmt, NULL);
	sqlite3_free(query);
	if ( ret != SQLITE_OK) {
		TRACE_ERROR("[PREPARE] [%s]", sqlite3_errmsg(g_dp_db_handle));
		__dp_finalize(stmt);
		return -1;
	}

	if (condtype == DP_DB_COL_TYPE_INT) {
		int *cast_value = condvalue;
		errorcode = sqlite3_bind_int(stmt, 2, *cast_value);
	} else if (condtype == DP_DB_COL_TYPE_INT64) {
#ifdef SQLITE_INT64_TYPE
		sqlite3_int64 *cast_value = condvalue;
		errorcode = sqlite3_bind_int64(stmt, 2, *cast_value);
#else
		int *cast_value = condvalue;
		errorcode = sqlite3_bind_int(stmt, 2, *cast_value);
#endif
	} else if (condtype == DP_DB_COL_TYPE_TEXT) {
		errorcode = sqlite3_bind_text(stmt, 2, (char*)condvalue, -1, NULL);
	} else {
		TRACE_ERROR("[CHECK TYPE] Not Support [%d]", condtype);
		__dp_finalize(stmt);
		return -1;
	}
	if (errorcode != SQLITE_OK) {
		TRACE_ERROR("[BIND] [%d] [%s]",
			condtype, sqlite3_errmsg(g_dp_db_handle));
		__dp_finalize(stmt);
		return -1;
	}

	if (sqlite3_bind_int(stmt, 1, id)
		!= SQLITE_OK) {
		TRACE_ERROR("sqlite3_bind_int[%s]",
			sqlite3_errmsg(g_dp_db_handle));
		__dp_finalize(stmt);
		return -1;
	}
	errorcode = sqlite3_step(stmt);
	if (errorcode == SQLITE_OK || errorcode == SQLITE_DONE) {
		__dp_finalize(stmt);
		return 0;
	}
	__dp_finalize(stmt);
	return -1;
}

int dp_db_get_cond_rows_count(int id, char *table,
						char *condcolumn, db_column_data_type condtype,
						void *condvalue)
{
	int errorcode = SQLITE_OK;
	int ret = -1;
	sqlite3_stmt *stmt = NULL;
	char *query = NULL;

	if (id <= 0) {
		TRACE_ERROR("[CHECK ID]");
		return -1;
	}

	if (!table) {
		TRACE_ERROR("[CHECK TABLE NAME]");
		return -1;
	}

	if (__dp_sql_open() < 0) {
		TRACE_ERROR("db_util_open is failed [%s]",
			sqlite3_errmsg(g_dp_db_handle));
		return -1;
	}

	if (condcolumn)
		query =
			sqlite3_mprintf
				("SELECT count(id) FROM %s WHERE id = ? AND %s = ?",
				table, condcolumn);
	else
		query =
			sqlite3_mprintf
				("SELECT count(id) FROM %s WHERE id = ?", table);

	if (query == NULL) {
		TRACE_ERROR("[CHECK COMBINE]");
		return -1;
	}

	TRACE_INFO("[QUERY] %s", query);

	ret = sqlite3_prepare_v2(g_dp_db_handle, query, -1, &stmt, NULL);
	sqlite3_free(query);
	if ( ret != SQLITE_OK) {
		TRACE_ERROR("[PREPARE] [%s]", sqlite3_errmsg(g_dp_db_handle));
		__dp_finalize(stmt);
		return -1;
	}

	if (condcolumn) {
		if (condtype == DP_DB_COL_TYPE_INT) {
			int *cast_value = condvalue;
			errorcode = sqlite3_bind_int(stmt, 2, *cast_value);
		} else if (condtype == DP_DB_COL_TYPE_INT64) {
#ifdef SQLITE_INT64_TYPE
			sqlite3_int64 *cast_value = condvalue;
			errorcode = sqlite3_bind_int64(stmt, 2, *cast_value);
#else
			int *cast_value = condvalue;
			errorcode = sqlite3_bind_int(stmt, 2, *cast_value);
#endif
		} else if (condtype == DP_DB_COL_TYPE_TEXT) {
			errorcode = sqlite3_bind_text(stmt, 2, (char*)condvalue, -1, NULL);
		} else {
			TRACE_ERROR("[CHECK TYPE] Not Support [%d]", condtype);
			__dp_finalize(stmt);
			return -1;
		}
		if (errorcode != SQLITE_OK) {
			TRACE_ERROR("[BIND] [%d] [%s]",
				condtype, sqlite3_errmsg(g_dp_db_handle));
			__dp_finalize(stmt);
			return -1;
		}
	}

	if (sqlite3_bind_int(stmt, 1, id)
		!= SQLITE_OK) {
		TRACE_ERROR("sqlite3_bind_int[%s]",
			sqlite3_errmsg(g_dp_db_handle));
		__dp_finalize(stmt);
		return -1;
	}

	errorcode = sqlite3_step(stmt);
	if (errorcode == SQLITE_ROW) {
		int count = sqlite3_column_int(stmt, 0);
		__dp_finalize(stmt);
		return count;
	}
	__dp_finalize(stmt);
	return 0;
}

int dp_db_get_http_headers_list(int id, char **headers)
{
	int errorcode = SQLITE_OK;
	int i = 0;
	int headers_index = 0;
	sqlite3_stmt *stmt = NULL;

	if (id <= 0) {
		TRACE_ERROR("[CHECK ID]");
		return -1;
	}

	if (__dp_sql_open() < 0) {
		TRACE_ERROR("db_util_open is failed [%s]",
			sqlite3_errmsg(g_dp_db_handle));
		return -1;
	}

	errorcode =
		sqlite3_prepare_v2(g_dp_db_handle,
			"SELECT header_field, header_data FROM httpheaders WHERE id = ?",
			-1, &stmt, NULL);
	if (errorcode != SQLITE_OK) {
		TRACE_ERROR("sqlite3_prepare_v2 is failed. [%s]",
			sqlite3_errmsg(g_dp_db_handle));
		__dp_finalize(stmt);
		return -1;
	}
	if (sqlite3_bind_int(stmt, 1, id) != SQLITE_OK) {
		TRACE_ERROR("sqlite3_bind_int [%s]",
			sqlite3_errmsg(g_dp_db_handle));
		__dp_finalize(stmt);
		return -1;
	}

	while ((errorcode = sqlite3_step(stmt)) == SQLITE_ROW) {
		int buffer_length = 0;
		char *header_field = (char *)(sqlite3_column_text(stmt, 0));
		char *header_data = (char *)(sqlite3_column_text(stmt, 1));
		i++;
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
