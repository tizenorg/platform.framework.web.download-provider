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

#ifndef DOWNLOAD_PROVIDER_NOTIFICATION_MANAGER_H
#define DOWNLOAD_PROVIDER_NOTIFICATION_MANAGER_H

typedef enum {
	DP_NOTIFICATION = 0,
	DP_NOTIFICATION_ONGOING,
	DP_NOTIFICATION_ONGOING_PROGRESS,
	DP_NOTIFICATION_ONGOING_UPDATE,
} dp_noti_type;

void dp_notification_manager_kill();
void dp_notification_manager_wake_up();
int dp_notification_manager_clear_notification(void *slot, void *request, const dp_noti_type type);
//int dp_notification_manager_push_notification_ongoing(void *slot, void *request, const dp_noti_type type);
int dp_notification_manager_push_notification(void *slot, void *request, const dp_noti_type type);

#endif
