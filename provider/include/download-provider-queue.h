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

#ifndef DOWNLOAD_PROVIDER_QUEUE_H
#define DOWNLOAD_PROVIDER_QUEUE_H

typedef struct { // manage clients without mutex
	void *slot; // client can not be NULL. it will exist in dummy
	void *request;
	void *next;
} dp_queue_fmt;

int dp_queue_push(dp_queue_fmt **queue, void *slot, void *request);
int dp_queue_pop(dp_queue_fmt **queue, void **slot, void **request);
void dp_queue_clear(dp_queue_fmt **queue, void *request);
void dp_queue_clear_all(dp_queue_fmt **queue);

#endif
