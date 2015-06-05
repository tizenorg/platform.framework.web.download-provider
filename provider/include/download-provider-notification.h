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

#ifndef DOWNLOAD_PROVIDER_NOTIFICATION_H
#define DOWNLOAD_PROVIDER_NOTIFICATION_H

#ifdef SUPPORT_NOTIFICATION
void dp_notification_set_locale(void);
void dp_notification_clear_locale(void);
void dp_notification_clear_ongoings(void);
int dp_notification_delete_ongoing(const int noti_priv_id);
int dp_notification_delete(const int noti_priv_id);
int dp_notification_ongoing_new(const char *pkgname, const char *subject, unsigned char *raws_buffer, const int raws_length);
int dp_notification_ongoing_update(const int noti_priv_id, const double received_size, const double file_size, const char *subject);
int dp_notification_new(void *dbhandle, const int download_id, const int state, int content_type, const char *pkgname);
#else
void dp_notification_clear_ongoings(void);
#endif

#endif
