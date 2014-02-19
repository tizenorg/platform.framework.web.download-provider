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

#include "download-agent-http-misc.h"
#include "download-agent-debug.h"
#include "download-agent-dl-mgr.h"
#include "download-agent-plugin-conf.h"
#include "download-agent-client-mgr.h"

#define DEFAULT_HTTP_ACCEPT_HEADERS \
		"Accept-Language: en\r\n" \
		"Accept-Charset: utf-8\r\n" \


char *get_user_agent()
{
	char *uagent_str = DA_NULL;

	DA_LOG_FUNC_LOGV(Default);

	uagent_str = get_client_user_agent_string();
	if (!uagent_str) {
		da_result_t ret = DA_RESULT_OK;
		ret = get_user_agent_string(&uagent_str);
		if (ret != DA_RESULT_OK)
			return NULL;
	}
	return uagent_str;
}

da_bool_t is_supporting_protocol(const char *protocol)
{
	if((protocol == NULL) || (1 > strlen(protocol)))
	{
		return DA_FALSE;
	}

	if(!strcasecmp(protocol, "http"))
	{
		return DA_TRUE;
	}
	else if(!strcasecmp(protocol, "https"))
	{
		return DA_TRUE;
	}
	else
	{
		return DA_FALSE;
	}

}
