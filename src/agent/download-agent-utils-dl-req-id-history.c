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
 * @file		download-agent-utils-dl-req-id-history.c
 * @brief		Including operations for dl-req-id-history
 * @author		Keunsoon Lee(keunsoon.lee@samsung.com)
 ***/

#include "download-agent-type.h"
#include "download-agent-utils.h"
#include "download-agent-utils-dl-req-id-history.h"

da_result_t init_dl_req_id_history(dl_req_id_history_t *dl_req_id_history)
{
	da_result_t ret = DA_RESULT_OK;

	/* Initial dl_req_id_history will be starting number for dl_req_id.
	 * dl_req_id will be sequentially increased from the dl_req_id_history,
	 * then dl_req_id_history will be updated. */
	_da_thread_mutex_init(&(dl_req_id_history->mutex), DA_NULL);
	_da_thread_mutex_lock(&(dl_req_id_history->mutex));
	get_random_number(&(dl_req_id_history->starting_num));
	dl_req_id_history->cur_dl_req_id = DA_INVALID_ID;
	_da_thread_mutex_unlock(&(dl_req_id_history->mutex));

	DA_LOG_CRITICAL(Default,"starting num = %d", dl_req_id_history->starting_num);
	return ret;
}

da_result_t deinit_dl_req_id_history(dl_req_id_history_t *dl_req_id_history)
{
	da_result_t ret = DA_RESULT_OK;

	_da_thread_mutex_lock(&(dl_req_id_history->mutex));
	dl_req_id_history->starting_num = DA_INVALID_ID;
	dl_req_id_history->cur_dl_req_id = DA_INVALID_ID;
	_da_thread_mutex_unlock(&(dl_req_id_history->mutex));

	_da_thread_mutex_destroy(&(dl_req_id_history->mutex));

	return ret;
}

int get_available_dl_req_id(dl_req_id_history_t *dl_req_id_history)
{
	int dl_req_id = 0;

	_da_thread_mutex_lock(&(dl_req_id_history->mutex));

	if (dl_req_id_history->cur_dl_req_id == DA_INVALID_ID)
		dl_req_id_history->cur_dl_req_id = dl_req_id_history->starting_num;
	else if (dl_req_id_history->cur_dl_req_id > 254)
		dl_req_id_history->cur_dl_req_id = 1;
	else
		dl_req_id_history->cur_dl_req_id++;

	dl_req_id = dl_req_id_history->cur_dl_req_id;

	_da_thread_mutex_unlock(&(dl_req_id_history->mutex));

	DA_LOG_CRITICAL(Default,"dl_req_id = %d", dl_req_id);
	return dl_req_id;
}
