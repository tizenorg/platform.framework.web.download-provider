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

#ifndef DOWNLOAD_PROVIDER_CLIENT_MANAGER_H
#define DOWNLOAD_PROVIDER_CLIENT_MANAGER_H

#include <download-provider.h>
#include <download-provider-client.h>

typedef struct {
	dp_credential credential;
	pthread_mutex_t mutex; // lock whenever access client variable
	pthread_t thread;
	char *pkgname;
	dp_client_fmt client;
} dp_client_slots_fmt;

int dp_client_slot_free(dp_client_slots_fmt *slot);
void dp_broadcast_signal();
char *dp_db_get_client_smack_label(const char *pkgname);

#endif
