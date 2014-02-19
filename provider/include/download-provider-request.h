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

#ifndef DOWNLOAD_PROVIDER2_REQUEST_H
#define DOWNLOAD_PROVIDER2_REQUEST_H

#include "download-provider.h"
#include <bundle.h>

// for Debugging
char *dp_print_state(dp_state_type state);
char *dp_print_errorcode(dp_error_type errorcode);

int dp_is_smackfs_mounted(void);
char *dp_strdup(char *src);

dp_error_type dp_request_create(int id, dp_client_group *group, dp_request** empty_slot);
dp_error_type dp_request_set_url(int id, dp_request *request, char *url);
dp_error_type dp_request_set_destination(int id, dp_request *request, char *dest);
dp_error_type dp_request_set_filename(int id, dp_request *request, char *filename);
dp_error_type dp_request_set_title(int id, dp_request *request, char *filename);
dp_error_type dp_request_set_bundle(int id, dp_request *request, int type, bundle_raw *b, unsigned length);
dp_error_type dp_request_set_description(int id, dp_request *request, char *description);
dp_error_type dp_request_set_noti_type(int id, dp_request *request, unsigned type);
dp_error_type dp_request_set_notification(int id, dp_request *request, unsigned enable);
dp_error_type dp_request_set_auto_download(int id, dp_request *request, unsigned enable);
dp_error_type dp_request_set_state_event(int id, dp_request *request, unsigned enable);
dp_error_type dp_request_set_progress_event(int id, dp_request *request, unsigned enable);
dp_error_type dp_request_set_network_type(int id, dp_request *request, int type);
char *dp_request_get_url(int id, dp_error_type *errorcode);
char *dp_request_get_destination(int id, dp_request *request, dp_error_type *errorcode);
char *dp_request_get_filename(int id, dp_request *request, dp_error_type *errorcode);
char *dp_request_get_title(int id, dp_request *request, dp_error_type *errorcode);
bundle_raw *dp_request_get_bundle(int id, dp_request *request, dp_error_type *errorcode, char* column, int* length);
char *dp_request_get_description(int id, dp_request *request, dp_error_type *errorcode);
int dp_request_get_noti_type(int id, dp_request *request, dp_error_type *errorcode);
char *dp_request_get_contentname(int id, dp_request *request, dp_error_type *errorcode);
char *dp_request_get_etag(int id, dp_request *request, dp_error_type *errorcode);
char *dp_request_get_savedpath(int id, dp_request *request, dp_error_type *errorcode);
char *dp_request_get_tmpsavedpath(int id, dp_request *request, dp_error_type *errorcode);
char *dp_request_get_mimetype(int id, dp_request *request, dp_error_type *errorcode);
char *dp_request_get_pkg_name(int id, dp_request *request, dp_error_type *errorcode);
dp_request *dp_request_load_from_log(int id, dp_error_type *errorcode);
void dp_request_state_response(dp_request *request);
#endif
