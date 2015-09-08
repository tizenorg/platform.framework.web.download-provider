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
 * @author      Oskar Świtalski <o.switalski@samsung.com>
 * @version     1.0
 * @brief       Implementation of cynara based access control functions
 */

#include <stdio.h>
#include <stdlib.h>

#include <cynara-client.h>
#include <cynara-session.h>
#include <cynara-creds-socket.h>

#include "download-provider-log.h"
#include "download-provider.h"

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

int dp_cynara_check(int fd, const char *priv)
{
	int ret = 0;
	pid_t pid = 0;
	char *user = NULL;
	char *client = NULL;
	char *session = NULL;

	ret = cynara_creds_socket_get_user(fd, USER_METHOD_DEFAULT, &user);
	if (ret != CYNARA_API_SUCCESS) {
		dp_cynara_log_error("cynara_creds_socket_get_user", ret);
		goto CLEANUP;
	}

	ret = cynara_creds_socket_get_client(fd, CLIENT_METHOD_DEFAULT, &client);
	if (ret != CYNARA_API_SUCCESS) {
		dp_cynara_log_error("cynara_creds_socket_get_client", ret);
		goto CLEANUP;
	}

	ret = cynara_creds_socket_get_pid(fd, &pid);
	if (ret != CYNARA_API_SUCCESS) {
		dp_cynara_log_error("cynara_creds_socket_get_pid", ret);
		goto CLEANUP;
	}

	session = cynara_session_from_pid(pid);
	if (!session) {
		TRACE_ERROR("[CYNARA] cynara_session_from_pid() failed");
		ret = CYNARA_API_UNKNOWN_ERROR;
		goto CLEANUP;
	}

	ret = cynara_check(p_cynara, client, session, user, priv);
	switch (ret) {
	case CYNARA_API_ACCESS_ALLOWED:
		TRACE_DEBUG("[CYNARA] Check (client = %s, session = %s, user = %s, privilege = %s )"
			"=> access allowed", client, session, user, priv);
		break;
	case CYNARA_API_ACCESS_DENIED:
		TRACE_DEBUG("[CYNARA] Check (client = %s, session = %s, user = %s, privilege = %s )"
			"=> access denied", client, session, user, priv);
		break;
	default:
		dp_cynara_log_error("cynara_check", ret);
	}

CLEANUP:
	if (user)
	  free(user);
	if (session)
	  free(session);
	if (client)
	  free(client);
	return ret;
}
