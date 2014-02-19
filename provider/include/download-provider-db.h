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

#ifndef DOWNLOAD_PROVIDER2_DB_H
#define DOWNLOAD_PROVIDER2_DB_H

#include "download-provider-config.h"
#include "download-provider-slots.h"

/*
 * Memory ( sync with logging ) : id, state, errorcode, startcount, packagename
 * DB TABLES
 * 				logging			: id, state, errorcode, startcount, createtime, accesstime, packagename
 * 				requestinfo		: id, auto_download, network_type, filename, destination, url
 * 				downloadinfo 	: id, http_status, content_size, mimetype, contentname, saved_path, tmp_saved_path, etag
 * 				httpheaders		: id, header_field, header_data
 * 				notification	: id, noti_enable, extra_key, extra_data
 */
/*
CREATE TABLE IF NOT EXISTS groups
(
        id             INTEGER UNIQUE PRIMARY KEY,
        uid            INTEGER DEFAULT 0,
        gid            INTEGER DEFAULT 0,
        extra_int      INTEGER DEFAULT 0,
        packagename    TEXT DEFAULT NULL,
        smack_label    TEXT DEFAULT NULL,
        extra          TEXT DEFAULT NULL,
        date_first_connected DATE,
        date_last_connected DATE
);

CREATE TABLE logging
(
        id              INTEGER UNIQUE PRIMARY KEY,
        state           INTEGER DEFAULT 0,
        errorcode       INTEGER DEFAULT 0,
        startcount      INTEGER DEFAULT 0,
        packagename     TEXT DEFAULT NULL,
        createtime      DATE,
        accesstime      DATE
);

CREATE TABLE requestinfo
(
        id              INTEGER UNIQUE PRIMARY KEY,
        auto_download   BOOLEAN DEFAULT 0,
        state_event     BOOLEAN DEFAULT 0,
        progress_event  BOOLEAN DEFAULT 0,
        network_type    TINYINT DEFAULT 0,
        filename        TEXT DEFAULT NULL,
        destination     TEXT DEFAULT NULL,
        url             TEXT DEFAULT NULL,
        FOREIGN KEY(id) REFERENCES logging(id) ON DELETE CASCADE
);

CREATE TABLE downloadinfo
(
        id              INTEGER UNIQUE PRIMARY KEY,
        http_status     INTEGER DEFAULT 0,
        content_size    UNSIGNED BIG INT DEFAULT 0,
        mimetype        VARCHAR(64) DEFAULT NULL,
        content_name    TEXT DEFAULT NULL,
        saved_path      TEXT DEFAULT NULL,
        tmp_saved_path  TEXT DEFAULT NULL,
        etag            TEXT DEFAULT NULL,
        FOREIGN KEY(id) REFERENCES logging(id) ON DELETE CASCADE
);

CREATE TABLE httpheaders
(
        id              INTEGER NOT NULL,
        header_field    TEXT DEFAULT NULL,
        header_data     TEXT DEFAULT NULL,
        FOREIGN KEY(id) REFERENCES logging(id) ON DELETE CASCADE
);

CREATE TABLE notification
(
        id              INTEGER UNIQUE PRIMARY KEY,
        noti_enable     BOOLEAN DEFAULT 0,
        extra_key       TEXT DEFAULT NULL,
        extra_data      TEXT DEFAULT NULL,
        FOREIGN KEY(id) REFERENCES logging(id) ON DELETE CASCADE
);

CREATE UNIQUE INDEX requests_index ON logging (id, state, errorcode, packagename, createtime, accesstime);
*/

#define DP_DB_TABLE_GROUPS "groups"
#define DP_DB_TABLE_LOG "logging"
#define DP_DB_TABLE_REQUEST_INFO "requestinfo"
#define DP_DB_TABLE_DOWNLOAD_INFO "downloadinfo"
#define DP_DB_TABLE_HTTP_HEADERS "httpheaders"
#define DP_DB_TABLE_NOTIFICATION "notification"

#define DP_DB_COL_ID "id"
#define DP_DB_COL_STATE "state"
#define DP_DB_COL_ERRORCODE "errorcode"
#define DP_DB_COL_NETWORK_TYPE "network_type"
#define DP_DB_COL_HTTP_STATUS "http_status"
#define DP_DB_COL_AUTO_DOWNLOAD "auto_download"
#define DP_DB_COL_STATE_EVENT "state_event"
#define DP_DB_COL_PROGRESS_EVENT "progress_event"
#define DP_DB_COL_CONTENT_SIZE "content_size"
#define DP_DB_COL_CREATE_TIME "createtime"
#define DP_DB_COL_ACCESS_TIME "accesstime"
#define DP_DB_COL_STARTCOUNT "startcount"
#define DP_DB_COL_PACKAGENAME "packagename"
#define DP_DB_COL_DESTINATION "destination"
#define DP_DB_COL_FILENAME "filename"
#define DP_DB_COL_CONTENT_NAME "content_name"
#define DP_DB_COL_MIMETYPE "mimetype"
#define DP_DB_COL_ETAG "etag"
#define DP_DB_COL_SAVED_PATH "saved_path"
#define DP_DB_COL_TMP_SAVED_PATH "tmp_saved_path"
#define DP_DB_COL_URL "url"
#define DP_DB_COL_HEADER_FIELD "header_field"
#define DP_DB_COL_HEADER_DATA "header_data"
#define DP_DB_COL_NOTIFICATION_ENABLE "noti_enable"
#define DP_DB_COL_EXTRA_KEY "extra_key"
#define DP_DB_COL_DISTINCT_EXTRA_KEY "DISTINCT extra_key"
#define DP_DB_COL_EXTRA_VALUE "extra_data"
#define DP_DB_COL_RAW_BUNDLE_ONGOING "raw_bundle_data_ongoing_state"
#define DP_DB_COL_RAW_BUNDLE_COMPLETE "raw_bundle_data_complete_state"
#define DP_DB_COL_RAW_BUNDLE_FAIL "raw_bundle_data_fail_state"
#define DP_DB_COL_TITLE "title"
#define DP_DB_COL_DESCRIPTION "description"
#define DP_DB_COL_NOTI_TYPE "noti_type"

#define DP_DB_GROUPS_COL_UID "uid"
#define DP_DB_GROUPS_COL_GID "gid"
#define DP_DB_GROUPS_COL_PKG "packagename"
#define DP_DB_GROUPS_COL_SMACK_LABEL "smack_label"


typedef enum {
	DP_DB_COL_TYPE_NONE = 0,
	DP_DB_COL_TYPE_INT = 10,
	DP_DB_COL_TYPE_INT64 = 20,
	DP_DB_COL_TYPE_TEXT = 30
} db_column_data_type;

typedef struct {
	char *column;
	db_column_data_type type;
	int is_like;
	void *value;
} db_conds_list_fmt;

int dp_db_open();
void dp_db_close();

int dp_db_is_full_error();

int dp_db_remove_all(int id);
int dp_db_remove(int id, char *table);
int dp_db_insert_column(int id, char *table, char *column,
						db_column_data_type datatype, void *value);
int dp_db_insert_blob_column(int id, char *table, char *column,
						void *value, unsigned length);
int dp_db_set_column(int id, char *table, char *column,
						db_column_data_type datatype, void *value);
int dp_db_set_blob_column(int id, char *table, char *column,
						void *value, unsigned length);
int dp_db_replace_column(int id, char *table, char *column,
						db_column_data_type datatype, void *value);
int dp_db_replace_blob_column(int id, char *table, char *column,
						void *value, unsigned length);
char *dp_db_get_text_column(int id, char *table, char *column);
void *dp_db_get_blob_column(int id, char *table, char *column, int *length);
int dp_db_get_int_column(int id, char *table, char *column);
long long dp_db_get_int64_column(int id, char *table, char *column);
int dp_db_update_date(int id, char *table, char *column);

// cond : id & cond
int dp_db_cond_set_column(int id, char *table, char *column,
						db_column_data_type datatype, void *value,
						char *condcolumn, db_column_data_type condtype,
						void *condvalue);
char *dp_db_cond_get_text_column(int id, char *table, char *column,
						char *condcolumn, db_column_data_type condtype,
						void *condvalue);
int dp_db_cond_remove(int id, char *table,
						char *condcolumn, db_column_data_type condtype,
						void *condvalue);
int dp_db_get_cond_rows_count(int id, char *table,
						char *condcolumn, db_column_data_type condtype,
						void *condvalue);

char *dp_db_cond_get_text(char *table, char *column, char *condcolumn,
						db_column_data_type condtype, void *condvalue);
int dp_db_cond_get_int(char *table, char *column, char *condcolumn,
						db_column_data_type condtype, void *condvalue);

// Special API for http headers
int dp_db_get_http_headers_list(int id, char **headers);

// For auto-download in booting time
int dp_db_crashed_list(dp_request_slots *requests, int limit);

// For loading to memory when no in memory
dp_request *dp_db_load_logging_request(int id);

// For limitation by 48 hours & 1000 rows
int dp_db_limit_rows(int limit);
int dp_db_get_count_by_limit_time();
int dp_db_get_list_by_limit_time(dp_request_slots *requests, int limit);

int dp_db_insert_columns(char *table, int column_count,
						db_conds_list_fmt *columns);
int dp_db_update_columns(int id, char *table, int column_count,
						db_conds_list_fmt *columns);

int dp_db_get_conds_rows_count(char *table, char *getcolumn, char *op,
						int conds_count, db_conds_list_fmt *conds);

int dp_db_get_conds_list(char *table, char *getcolumn,
						db_column_data_type gettype, void **list,
						int rowslimit, int rowsoffset,
						char *ordercolumn, char *ordering,
						char *op, int conds_count,
						db_conds_list_fmt *conds);

int dp_db_request_new_logging(const int id, const int state, const char *pkgname);
int dp_db_request_update_status(const int id, const int state, const int download_error);
int dp_db_get_state(int id);
int dp_db_request_new_logging(const int id,
						const int state, const char *pkgname);
#endif
