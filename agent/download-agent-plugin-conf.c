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

#include <string.h>
#include <stdlib.h>
#include <glib-object.h>

#ifdef _EFL_PLATFORM
#include <vconf.h>
#include <vconf-keys.h>
#include <net_connection.h>
#endif /* _EFL_PLATFORM */

#include "download-agent-plugin-conf.h"
#include "download-agent-debug.h"
#include "download-agent-file.h"

#define DEFAULT_UA_STR "Mozilla/5.0 (Linux; U; Tizen 1.0; en-us) AppleWebKit/534.46 (KHTML, like Gecko) Mobile Tizen Browser/1.0"

da_result_t __get_conf_string(const char *key, char **out_string);

da_result_t __get_conf_string(const char *key, char **out_string)
{
#ifdef _EFL_PLATFORM
	if (!key || !out_string) {
		DA_LOG_ERR(Default,"Invalid Argument");
		return DA_ERR_INVALID_ARGUMENT;
	}

	*out_string = vconf_get_str(key);
	return DA_RESULT_OK;
#else
	if (out_string)
		*out_string = NULL;

	return DA_RESULT_OK;
#endif
}

da_result_t get_user_agent_string(char **uagent_str)
{
	da_result_t  ret = DA_RESULT_OK;
#ifdef _EFL_PLATFORM
	char *key = DA_NULL;
#endif

	DA_LOG_FUNC_LOGV(Default);

	if (!uagent_str) {
		DA_LOG_ERR(Default,"Invalid Argument");
		return DA_ERR_INVALID_ARGUMENT;
	}

#ifdef _EFL_PLATFORM
	key = VCONFKEY_BROWSER_USER_AGENT;
	ret = __get_conf_string(key, uagent_str);
	if(ret == DA_RESULT_OK) {
		if(*uagent_str) {
			DA_SECURE_LOGD("getting uagent_str = \n%s", *uagent_str);
			return ret;
		}
	}
	DA_LOG_ERR(Default,"No UA information from vconf !!");
	*uagent_str = strdup(DEFAULT_UA_STR);
	DA_LOG(Default,"Set default UA");
#else
	*uagent_str = strdup(DEFAULT_UA_STR);
#endif
	return ret;
}

char *get_proxy_address(void)
{
#ifdef _EFL_PLATFORM
	char *proxy = NULL;
	char *proxyRet = NULL;
	connection_h handle = NULL;
    connection_address_family_e family = CONNECTION_ADDRESS_FAMILY_IPV4;

    DA_LOG_FUNC_LOGV(Default);
    if (connection_create(&handle) < 0) {
		DA_LOG_ERR(Default,"Fail to create connection handle");
		return NULL;
	}

	if (connection_get_proxy(handle, family, &proxyRet) < 0) {
		DA_LOG_ERR(Default,"Fail to get proxy address");
		connection_destroy(handle);
		return NULL;
	}

	if (proxyRet) {
		DA_SECURE_LOGD("===== Proxy address[%s] =====", proxyRet);
		proxy = strdup(proxyRet);
		free(proxyRet);
		proxyRet = NULL;
		connection_destroy(handle);
		return proxy;
	}

    if (connection_destroy(handle) < 0) {
		DA_LOG_ERR(Default,"Fail to desctory connection handle");
		return NULL;
	}
	return NULL;
#else
	return NULL;
#endif
}
