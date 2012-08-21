/*
 * Download Agent
 *
 * Copyright (c) 2000 - 2012 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact: Jungki Kwak <jungki.kwak@samsung.com>, Keunsoon Lee <keunsoon.lee@samsung.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * @file		download-agent-utils-dl-req-id-history.h
 * @brief		Including operations for dl-req-id-history
 * @author		Keunsoon Lee(keunsoon.lee@samsung.com)
 ***/


#ifndef _Download_Agent_Utils_Hash_Table_H
#define _Download_Agent_Utils_Hash_Table_H

#include "download-agent-pthread.h"

typedef struct _dl_req_id_history_t dl_req_id_history_t;
struct _dl_req_id_history_t {
	int starting_num;
	int cur_dl_req_id;
	pthread_mutex_t mutex;
};

da_result_t init_dl_req_id_history(dl_req_id_history_t *dl_req_id_history);
da_result_t deinit_dl_req_id_history(dl_req_id_history_t *dl_req_id_history);

int get_available_dl_req_id(dl_req_id_history_t *dl_req_id_history);


#endif /* _Download_Agent_Utils_Hash_Table_H */
