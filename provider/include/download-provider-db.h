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

#ifndef DOWNLOAD_PROVIDER_DB_H
#define DOWNLOAD_PROVIDER_DB_H

int dp_db_check_connection(void *handle);
int dp_db_open_client_manager(void **handle);
int dp_db_open_client(void **handle, char *pkgname);
int dp_db_open_client_v2(void **handle, char *pkgname);
int dp_db_remove_database(char *pkgname, long now_time, long diff_time);
int dp_db_get_ids(void *handle, const char *table, char *idcolumn, int *ids, const char *where, const int limit, char *ordercolumn, char *ordering, int *error);
int dp_db_get_crashed_ids(void *handle, const char *table, int *ids, const int limit, int *error);
void dp_db_close(void *handle);
void dp_db_reset(void *stmt);
void dp_db_finalize(void *stmt);
int dp_db_get_errorcode(void *handle);

int dp_db_check_duplicated_int(void *handle, const char *table, const char *column, const int value, int *error);
int dp_db_check_duplicated_string(void *handle, const int id, const char *table, const char *column, const int is_like, const char *value, int *error);
int dp_db_update_client_info(void *handle, const char *pkgname, const char *smack, const int uid, const int gid, int *error);
int dp_db_get_client_property_string(void *handle, const char *pkgname, const char *column, unsigned char **value, unsigned *length, int *error);
int dp_db_new_logging(void *handle, const int id, const int state, const int errorvalue, int *error);
int dp_db_update_logging(void *handle, const int id, const int state, const int errorvalue, int *error);
int dp_db_replace_property(void *handle, const int id, const char *table, const char *column, const void *value, const unsigned length, const unsigned valuetype, int *error);
int dp_db_get_property_string(void *handle, const int id, const char *table, const char *column, unsigned char **value, unsigned *length, int *error);
int dp_db_get_property_int(void *handle, const int id, const char *table, const char *column, void *value, int *error);
int dp_db_unset_property_string(void *handle, const int id, const char *table, const char *column, int *error);
int dp_db_delete(void *handle, const int id, const char *table, int *error);


int dp_db_new_header(void *handle, const int id, const char *field, const char *value, int *error);
int dp_db_update_header(void *handle, const int id, const char *field, const char *value, int *error);
int dp_db_get_header_value(void *handle, const int id, const char *field, unsigned char **value, unsigned *length, int *error);
int dp_db_cond_delete(void *handle, const int id, const char *table, const char *column, const void *value, const unsigned valuetype, int *error);
int dp_db_get_cond_ids(void *handle, const char *table, const char *getcolumn, const char *column, const int value, int *ids, const int limit, int *error);
int dp_db_get_cond_string(void *handle, const char *table, char *wherecolumn, const int wherevalue, const char *getcolumn, unsigned char **value, unsigned *length, int *error);
int dp_db_limit_rows(void *handle, const char *table, int limit, int *error);
int dp_db_limit_time(void *handle, const char *table, int hours, int *error);
int dp_db_get_http_headers_list(void *handle, int id, char **headers, int *error);

#endif
