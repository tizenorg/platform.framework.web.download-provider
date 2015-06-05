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

#ifndef _DOWNLOAD_AGENT_DEFS_H
#define _DOWNLOAD_AGENT_DEFS_H

#ifndef DEPRECATED
#define DEPRECATED __attribute__((deprecated))
#endif

/**
 * Max count to download files simultaneously. \n
 * Main reason for this restriction is because of Network bandwidth.
 */
#define DA_MAX_DOWNLOAD_REQ_AT_ONCE	50
#define DA_MAX_TIME_OUT					65

#define DA_RESULT_OK	0

#define DA_TRUE		1
#define DA_FALSE  		0
#define DA_NULL  		(void *)0
#define DA_INVALID_ID	-1

#define DA_RESULT_USER_CANCELED -10 

// InputError Input error (-100 ~ -199)
// Client passed wrong parameter
#define DA_ERR_INVALID_ARGUMENT		-100
#define DA_ERR_INVALID_DL_REQ_ID	-101
#define DA_ERR_INVALID_URL			-103
#define DA_ERR_INVALID_INSTALL_PATH	-104
#define DA_ERR_INVALID_MIME_TYPE	-105

// Client passed correct parameter, but Download Agent rejects the request because of internal policy.
#define DA_ERR_ALREADY_CANCELED		-160
#define DA_ERR_ALREADY_SUSPENDED	-161
#define DA_ERR_ALREADY_RESUMED		-162
#define DA_ERR_CANNOT_SUSPEND    	-170
#define DA_ERR_CANNOT_RESUME    	-171
#define DA_ERR_INVALID_STATE		-190
#define DA_ERR_ALREADY_MAX_DOWNLOAD	-191
#define DA_ERR_UNSUPPORTED_PROTOCAL	-192

// System error (-200 ~ -299)
#define DA_ERR_FAIL_TO_MEMALLOC		-200
#define DA_ERR_FAIL_TO_CREATE_THREAD		-210
#define DA_ERR_FAIL_TO_ACCESS_FILE	-230
#define DA_ERR_DISK_FULL  	-240

// Network error (-400 ~ -499)
#define DA_ERR_NETWORK_FAIL				-400
#define DA_ERR_UNREACHABLE_SERVER		-410
#define DA_ERR_CONNECTION_FAIL			-420
#define DA_ERR_HTTP_TIMEOUT         	-430
#define DA_ERR_SSL_FAIL					-440
#define DA_ERR_TOO_MANY_REDIRECTS		-450
#define DA_ERR_NETWORK_UNAUTHORIZED	-460

// HTTP error - not conforming with HTTP spec (-500 ~ -599)
#define DA_ERR_MISMATCH_CONTENT_TYPE	-500
#define DA_ERR_SERVER_RESPOND_BUT_SEND_NO_CONTENT	-501
#define DA_ERR_MISMATCH_CONTENT_SIZE	-502

// DRM error - not conforming with DRM spec (-600 ~ -699)
#define DA_ERR_DRM_FAIL			-600

// string to check invalid characters in path before using open() and fopen() API's
#define DA_INVALID_PATH_STRING ";\\\":*?<>|()"

#endif

