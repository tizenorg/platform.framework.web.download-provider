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

#ifndef DOWNLOAD_PROVIDER_DB_DEFS_H
#define DOWNLOAD_PROVIDER_DB_DEFS_H

#define DP_DBFILE_CLIENTS ".download-provider-clients"

// provider have a groups database file.
#define DP_TABLE_CLIENTS "clients"
// each client has a database file with below tables. file is named as pkgname.
#define DP_TABLE_LOGGING "logging"
#define DP_TABLE_REQUEST "request"
#define DP_TABLE_DOWNLOAD "download"
#define DP_TABLE_HEADERS "header"
#define DP_TABLE_NOTIFICATION "notification"

// common
#define DP_DB_COL_ROW_ID "ROWID"
#define DP_DB_COL_ID "id"
#define DP_DB_COL_CREATE_TIME "createtime"
#define DP_DB_COL_ACCESS_TIME "accesstime"

// clients table
#define DP_DB_COL_SMACK_LABEL "smack_label"
#define DP_DB_COL_PACKAGE "package"
#define DP_DB_COL_UID "uid"
#define DP_DB_COL_GID "gid"
#define DP_DB_COL_REQUEST_COUNT "requests"

// logging table
#define DP_DB_COL_STATE "state"
#define DP_DB_COL_ERRORCODE "errorcode"
#define DP_DB_COL_AUTO_DOWNLOAD "auto_download"
#define DP_DB_COL_STARTCOUNT "startcount"

// request table
#define DP_DB_COL_URL "url"
#define DP_DB_COL_DESTINATION "destination"
#define DP_DB_COL_FILENAME "filename"
#define DP_DB_COL_STATE_EVENT "state_event"
#define DP_DB_COL_PROGRESS_EVENT "progress_event"
#define DP_DB_COL_NETWORK_TYPE "network_type"
#define DP_DB_COL_NETWORK_BONDING "network_bonding"
#define DP_DB_COL_TEMP_FILE_PATH "temp_file_path"

// download table
#define DP_DB_COL_SAVED_PATH "saved_path"
#define DP_DB_COL_TMP_SAVED_PATH "tmp_saved_path"
#define DP_DB_COL_MIMETYPE "mimetype"
#define DP_DB_COL_CONTENT_NAME "content_name"
#define DP_DB_COL_ETAG "etag"
#define DP_DB_COL_CONTENT_SIZE "content_size"
#define DP_DB_COL_HTTP_STATUS "http_status"

// notification table
#define DP_DB_COL_NOTI_TYPE "type"
#define DP_DB_COL_NOTI_SUBJECT "subject"
#define DP_DB_COL_NOTI_DESCRIPTION "description"
#define DP_DB_COL_NOTI_PRIV_ID "priv_id"
#define DP_DB_COL_NOTI_RAW_ONGOING "raw_ongoing"
#define DP_DB_COL_NOTI_RAW_COMPLETE "raw_completed"
#define DP_DB_COL_NOTI_RAW_FAIL "raw_failed"

// http headers table
#define DP_DB_COL_HEADER_FIELD "header_field"
#define DP_DB_COL_HEADER_DATA "header_data"



// when a client is accepted, add
// when disconnected with no request, clear
// if exist, it's possible to be remain some requests
#define DP_SCHEMA_CLIENTS "CREATE TABLE IF NOT EXISTS clients(\
id INTEGER UNIQUE PRIMARY KEY,\
package TEXT UNIQUE NOT NULL,\
smack_label TEXT DEFAULT NULL,\
uid INTEGER DEFAULT 0,\
gid INTEGER DEFAULT 0,\
requests INTEGER DEFAULT 0,\
createtime DATE,\
accesstime DATE\
)"

// limitation : 1000 rows, 48 hours standard by createtime
#define DP_SCHEMA_LOGGING "CREATE TABLE IF NOT EXISTS logging(\
id INTEGER UNIQUE PRIMARY KEY DESC NOT NULL,\
state INTEGER DEFAULT 0,\
errorcode INTEGER DEFAULT 0,\
auto_download BOOLEAN DEFAULT 0,\
startcount INTEGER DEFAULT 0,\
createtime DATE,\
accesstime DATE\
)"

#define DP_SCHEMA_REQUEST "CREATE TABLE IF NOT EXISTS request(\
id INTEGER UNIQUE PRIMARY KEY,\
state_event BOOLEAN DEFAULT 0,\
progress_event BOOLEAN DEFAULT 0,\
network_type TINYINT DEFAULT 3,\
filename TEXT DEFAULT NULL,\
destination TEXT DEFAULT NULL,\
url TEXT DEFAULT NULL,\
temp_file_path TEXT DEFAULT NULL,\
network_bonding BOOLEAN DEFAULT 0,\
FOREIGN KEY(id) REFERENCES logging(id) ON DELETE CASCADE\
)"

#define DP_SCHEMA_DOWNLOAD "CREATE TABLE IF NOT EXISTS download(\
id INTEGER UNIQUE PRIMARY KEY,\
http_status INTEGER DEFAULT 0,\
content_size UNSIGNED BIG INT DEFAULT 0,\
mimetype VARCHAR(64) DEFAULT NULL,\
content_name TEXT DEFAULT NULL,\
saved_path TEXT DEFAULT NULL,\
tmp_saved_path TEXT DEFAULT NULL,\
etag TEXT DEFAULT NULL,\
FOREIGN KEY(id) REFERENCES logging(id) ON DELETE CASCADE\
)"

#define DP_SCHEMA_NOTIFICATION "CREATE TABLE IF NOT EXISTS notification(\
id INTEGER UNIQUE PRIMARY KEY,\
subject TEXT DEFAULT NULL,\
description TEXT DEFAULT NULL,\
type INTEGER DEFAULT 0,\
priv_id INTEGER DEFAULT -1,\
raw_completed BLOB DEFAULT NULL,\
raw_failed BLOB DEFAULT NULL,\
raw_ongoing BLOB DEFAULT NULL,\
FOREIGN KEY(id) REFERENCES logging(id) ON DELETE CASCADE\
)"

#define DP_SCHEMA_HEADER "CREATE TABLE IF NOT EXISTS header(\
id INTEGER NOT NULL,\
header_field TEXT DEFAULT NULL,\
header_data TEXT DEFAULT NULL,\
FOREIGN KEY(id) REFERENCES logging(id) ON DELETE CASCADE\
)"

#define DP_SCHEMA_LOGGING_INDEX  "CREATE UNIQUE INDEX IF NOT EXISTS requests_index ON logging (id, state, errorcode, createtime, accesstime)"



#endif
