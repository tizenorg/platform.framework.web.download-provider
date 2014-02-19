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

#include "download-agent-type.h"
#include "download-agent-utils.h"
#include "download-agent-utils-dl-id-history.h"

da_result_t init_dl_id_history(dl_id_history_t *dl_id_history)
{
	da_result_t ret = DA_RESULT_OK;

	/* Initial dl_id_history will be starting number for dl_id.
	 * dl_id will be sequentially increased from the dl_id_history,
	 * then dl_id_history will be updated. */
	_da_thread_mutex_init(&(dl_id_history->mutex), DA_NULL);
	_da_thread_mutex_lock(&(dl_id_history->mutex));
	get_random_number(&(dl_id_history->starting_num));
	dl_id_history->cur_dl_id = DA_INVALID_ID;
	_da_thread_mutex_unlock(&(dl_id_history->mutex));

	DA_LOG_VERBOSE(Default,"starting num = %d", dl_id_history->starting_num);
	return ret;
}

da_result_t deinit_dl_id_history(dl_id_history_t *dl_id_history)
{
	da_result_t ret = DA_RESULT_OK;

	_da_thread_mutex_lock(&(dl_id_history->mutex));
	dl_id_history->starting_num = DA_INVALID_ID;
	dl_id_history->cur_dl_id = DA_INVALID_ID;
	_da_thread_mutex_unlock(&(dl_id_history->mutex));

	_da_thread_mutex_destroy(&(dl_id_history->mutex));

	return ret;
}

int get_available_dl_id(dl_id_history_t *dl_id_history)
{
	int dl_id = 0;

	_da_thread_mutex_lock(&(dl_id_history->mutex));

	if (dl_id_history->cur_dl_id == DA_INVALID_ID)
		dl_id_history->cur_dl_id = dl_id_history->starting_num;
	else if (dl_id_history->cur_dl_id > 254)
		dl_id_history->cur_dl_id = 1;
	else
		dl_id_history->cur_dl_id++;

	dl_id = dl_id_history->cur_dl_id;

	_da_thread_mutex_unlock(&(dl_id_history->mutex));

	DA_LOG_VERBOSE(Default,"dl_id = %d", dl_id);
	return dl_id;
}
