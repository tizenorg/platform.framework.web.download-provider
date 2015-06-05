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

#ifndef DOWNLOAD_PROVIDER_NOTIFY_H
#define DOWNLOAD_PROVIDER_NOTIFY_H

int dp_notify_init(pid_t pid);
void dp_notify_deinit(pid_t pid);
int dp_notify_feedback(int sock, void *slot, int id, int state, int errorcode, unsigned long long received_size);

#endif
