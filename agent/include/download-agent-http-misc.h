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

#ifndef _Download_Agent_Http_Misc_H
#define _Download_Agent_Http_Misc_H

#include <string.h>

#include "download-agent-type.h"

#define SCHEME_HTTP		"http://"
#define SCHEME_HTTPS	"https://"
#define SCHEME_CID	"cid:"

#define METHOD_GET	"GET"
#define METHOD_POST	"POST"
#define METHOD_HEAD	"HEAD"

#define HTTP_TAG_UAGENT			"User-Agent: "
#define HTTP_TAG_HOST			"Host: "
#define HTTP_TAG_UAPROF			"X-Wap-Profile: "
#define HTTP_TAG_CONTENT_LENGTH	"Content-Length: "
#define HTTP_TAG_CONTENT_TYPE	"Content-Type: "
#define HTTP_TAG_IF_MATCH	"If-Match: "
#define HTTP_TAG_RANGE		"Range: "
#define HTTP_TAG_IF_RANGE	"If-Range: "

#define END_OF_FIELD		"\r\n"

char *get_user_agent();

da_bool_t is_supporting_protocol(const char *protocol);

#endif
