/*
 *  Copyright (c) 2015 Samsung Electronics Co., Ltd All Rights Reserved
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License
 */
/**
 * @file        download-provider-cynara.c
 * @author      Lukasz Wojciechowski <l.wojciechow@partner.samsung.com>
 * @version     1.0
 * @brief       Implementation of cynara based access control functions
 */

#include <stdio.h>
#include <stdlib.h>

#include <cynara-client.h>
#include <cynara-session.h>

#include "download-provider-log.h"

#define CYNARA_CACHE_SIZE 1000U

static cynara *p_cynara = NULL;

void dp_cynara_log_error(const char *function, int errorCode)
{
	char buffer[BUFSIZ] = {0};
	cynara_strerror(errorCode, buffer, sizeof(buffer));
	TRACE_ERROR("[CYNARA] %s function failed with error %d : %s", function, errorCode, buffer);
}

int dp_cynara_new()
{
	int ret, error = -1;
	cynara_configuration *p_conf = NULL;

	if (p_cynara) {
		TRACE_ERROR("[CYNARA] Improper usage of cynara");
		goto end;
	}

	ret = cynara_configuration_create(&p_conf);
	if (ret != CYNARA_API_SUCCESS) {
		dp_cynara_log_error("cynara_configuration_create", ret);
		goto end;
	}

	ret = cynara_configuration_set_cache_size(p_conf, CYNARA_CACHE_SIZE);
	if (ret != CYNARA_API_SUCCESS) {
		dp_cynara_log_error("cynara_configuration_set_cache_size", ret);
		goto end;
	}

	ret = cynara_initialize(&p_cynara, p_conf);
	if (ret != CYNARA_API_SUCCESS) {
		dp_cynara_log_error("cynara_initialize", ret);
		goto end;
	}

	error = 0;

end:
	cynara_configuration_destroy(p_conf);
	return error;
}

void dp_cynara_free()
{
	cynara_finish(p_cynara);
	p_cynara = NULL;
}

int dp_cynara_check(const char *client, const char *session, const char *user, const char *priv)
{
	if (client == NULL || session == NULL || user == NULL || priv == NULL) {
		TRACE_ERROR("[CYNARA] Bad parameters");
		return -1;
	}

	int ret = cynara_check(p_cynara, client, session, user, priv);

	switch(ret)
	{
	case CYNARA_API_ACCESS_ALLOWED:
		TRACE_DEBUG("[CYNARA] Check (client = %s, session = %s, user = %s, privilege = %s )"
			"=> access allowed", client, session, user, priv);
		return 0;
	case CYNARA_API_ACCESS_DENIED:
		TRACE_DEBUG("[CYNARA] Check (client = %s, session = %s, user = %s, privilege = %s )"
			"=> access denied", client, session, user, priv);
		return -1;
	default:
		dp_cynara_log_error("cynara_check", ret);
		return -1;
	}
}

const char *dp_cynara_user_new(uid_t uid)
{
	const char *user;
	int bytes = snprintf(NULL, 0, "%u", uid);
	if (bytes < 0) {
		TRACE_ERROR("Error in snprintf function");
		return NULL;
	}
	user = calloc(bytes + 1, 1);
	if (user == NULL) {
		TRACE_ERROR("Memory allocation error in calloc function");
		return NULL;
	}
	if (snprintf(user, bytes + 1, "%u", uid) < 0) {
		free(user);
		TRACE_ERROR("Error in snprintf function");
		return NULL;
	}
	return user;
}

const char *dp_cynara_session_new(pid_t pid)
{
	return cynara_session_from_pid(pid);
}
