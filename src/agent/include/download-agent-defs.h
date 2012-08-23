/**
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
 * @file		download-agent-defs.h
 * @brief		including types and defines for Download Agent
 * @author		Keunsoon Lee(keunsoon.lee@samsung.com)
 * @author		Jungki Kwak(jungki.kwak@samsung.com)
 */

#ifndef _Download_Agent_Defs_H
#define _Download_Agent_Defs_H

/**
 * @{
 */
#ifndef DEPRECATED
#define DEPRECATED __attribute__((deprecated))
#endif

/**
 * @ingroup Reference
 */
typedef int da_handle_t;

/**
 * @ingroup Reference
 * Max count to download files simultaneously. \n
 * Main reason for this restriction is because of Network bandwidth.
 */
#define DA_MAX_DOWNLOAD_REQ_AT_ONCE	5

/**
 * @ingroup Reference
 */
#define DA_RESULT_OK	0

#define DA_TRUE		1
#define DA_FALSE  		0
#define DA_NULL  		0
#define DA_INVALID_ID	-1

/**
 * @ingroup Reference
 * special \c dl_req_id to indicate all \c dl_req_id on current status \n\n
 * If client wants to do something (cancel/suspend/resume) for all \c dl_req_id, simply use this. \n
 * This sends a command to all downloading threads on current status without any changes on state notification. \n
 * That is, it is same with calling each dl_req_id separately.
 * @see DA_DOWNLOAD_REQ_ID_FOR_ALL_ITEMS_WITH_UNIFIED_NOTI
 */
#define DA_DOWNLOAD_REQ_ID_FOR_ALL_ITEMS -1
/**
 * @ingroup Reference
 * This is also indicates all \c dl_req_id, but it will not send state notification for each \c dl_req_id. \n
 * Instead, DA_STATE_XXX_ALL will be sent.
 * @see DA_DOWNLOAD_REQ_ID_FOR_ALL_ITEMS
 */
#define DA_DOWNLOAD_REQ_ID_FOR_ALL_ITEMS_WITH_UNIFIED_NOTI -2
/**
 * @}
 */

/**
 * @ingroup Reference
 * @addtogroup ExtensionFeatures Extension features
 * @{
 * @brief Download Agent's extension features using by da_start_download_with_extension()
 *
 * When calling \a da_start_download_with_extension function, use these defines for property name. \n
 *
*/
/**
 * @fn DA_FEATURE_REQUEST_HEADER
 * @brief receiving reqeust header for requesting URL on \a da_start_download_with_extension.
 * @remarks
 * 	property value type for this is 'char**'.
 *  value is valid if it has the string array which consist the name of http header field and the value of http header.
 *  ex) Cookie: SID=1234
 * @see da_start_download_with_extension
 */
#define DA_FEATURE_REQUEST_HEADER "request_header"

/**
 * @def DA_FEATURE_USER_DATA
 * @brief receiving user data for requesting URL on \a da_start_download_with_extension.
 * @remarks
 * 	property value type for this is 'void*'.
 * @details
 * 	Every client callback will convey this value.
 * @see da_start_download_with_extension
 */
#define DA_FEATURE_USER_DATA    "user_data"

/**
 * @def DA_FEATURE_INSTALL_PATH
 * @brief Downloaded file will be installed on designated path.
 * @remarks
 * 	property value type for this is 'char*'.
 * @warning
 * 	If the path is invalid, DA_ERR_INVALID_INSTALL_PATH will be returned. \n
 * 	No file name accepts, but only path does.
 * @see da_start_download_with_extension
 */
#define DA_FEATURE_INSTALL_PATH	"install_path"

/**
 * @def DA_FEATURE_FILE_NAME
 * @brief Downloaded file will be stored with the designated name.
 * @remarks
 * 	property value type for this is 'char*'.
 * @warning
 * 	No path accepts, but only file name dose. \n\n
 * 	If the designated file name does not include extension, DA will extract an extension automatically and use it. \n
 * 	If the designated file name includes extension, DA will use the extension without any check. \n\n
 * 	<b>It is not guaranteed to play or execute the stored file if client designates wrong extension. </b><br>
 * 	<b>If you are not confident of extension, DO NOT designate extension, but send just pure file name. </b><br><br>
 * 	<b>Client MUST check \a saved_path of \a user_download_info_t. </b><br>
 * 	<b>Because really decided file name can be different from the designated one. </b><br>
 * 	For example, actual file name can have numbering postfix if another file has same name is exist on storing directory. \n
 * 	(e.g. abc.mp3 to abc_1.mp3) \n\n
 * 	This feature is ignored in case of OMA/Midlet download. \n
 * @see da_start_download_with_extension
 * @see user_download_info_t
 */
#define DA_FEATURE_FILE_NAME	"file_name"
/**
*@}
*/

/**
 * @ingroup Reference
 * DA_DOWNLOAD_MANAGING_METHOD_MANUAL means that DA pass chunked packetes and response header data to the client in direct.
 * This is only availalbe in case of A link download (HTTP)
 * Please refer to sample code (da-test/md-sample.c)
 */
typedef enum {
	DA_DOWNLOAD_MANAGING_METHOD_AUTO = 0,	// DA handle all download progress including install a content. Support A link(HTTP) / OMA / MIDP download.
	DA_DOWNLOAD_MANAGING_METHOD_MANUAL,		// DA pass only chuncked packets, response header data, state and error code to the client. Support only A link (HTTP) download
	DA_DOWNLOAD_MANAGING_METHOD_MAX
} da_download_managing_method;

/**
 * @warning depricated
 */
typedef enum {
	DA_INSTALL_FAIL_REASON_OK = 0,				// install is succeed
	DA_INSTALL_FAIL_REASON_INSUFFICIENT_MEMORY,	// There is no sufficient memory on target
	DA_INSTALL_FAIL_REASON_USER_CANCELED,		// user canceled installation
	DA_INSTALL_FAIL_REASON_MAX
} da_install_fail_reason;


/**
 * @ingroup Reference
 * Download Agent notifies these states to client with \a da_notify_cb, when it is changed. \n
 * Most of these value are informative, except for DA_STATE_WAITING_USER_CONFIRM. \n
 * @remark Guarantee that one of following state is the final one.
 * 	@li DA_STATE_FINISHED
 * 	@li DA_STATE_CANCELED
 * 	@li DA_STATE_FAILED
 * @see user_notify_info_t
 * @see da_notify_cb
 * @par
	@li For default download
	\n DA_STATE_DOWNLOAD_STARTED,
	DA_STATE_DOWNLOADING,
	DA_STATE_DOWNLOAD_COMPLETE,

	@li For cancel, suspend, resume
	\n DA_STATE_CANCELED,
	DA_STATE_SUSPENDED,
	DA_STATE_RESUMED,

	@li Last notification for all case
	\n DA_STATE_FINISHED,
	DA_STATE_FAILED,
 */
typedef enum {
	DA_STATE_WAITING = 0,
	/// Requested download to Web server for designated URL
	DA_STATE_DOWNLOAD_STARTED,		//	1
	/// Started to receiving HTTP body data
	DA_STATE_DOWNLOADING,			//	2
	/// Completed to download from Web server. Not yet registered to system.
	DA_STATE_DOWNLOAD_COMPLETE,		//	3
	/// Suspended download
	DA_STATE_SUSPENDED,			//	4
	/// Suspended all download. Emitted only if receiving DA_DOWNLOAD_REQ_ID_FOR_ALL_ITEMS_WITH_UNIFIED_NOTI.
	DA_STATE_SUSPENDED_ALL,		// 5
	/// Resumed download which was suspended
	DA_STATE_RESUMED,			//	6
	/// Resumed all download. Emitted only if receiving DA_DOWNLOAD_REQ_ID_FOR_ALL_ITEMS_WITH_UNIFIED_NOTI.
	DA_STATE_RESUMED_ALL,		// 7
	/// Finished all process to download.
	DA_STATE_FINISHED,			//	8
	/// Canceled download
	DA_STATE_CANCELED,			//	9
	/// Canceled all download. Emitted only if receiving DA_DOWNLOAD_REQ_ID_FOR_ALL_ITEMS_WITH_UNIFIED_NOTI.
	DA_STATE_CANCELED_ALL,		// 10
	/// Failed to download
	DA_STATE_FAILED			//	11
} da_state;


/**
 * @ingroup Reference
 * @addtogroup ErrorCodes Error codes
 * @{
 */

/**
 * @{
 */
// InputError Input error (-100 ~ -199)
// Client passed wrong parameter
#define DA_ERR_INVALID_ARGUMENT		-100
#define DA_ERR_INVALID_DL_REQ_ID	-101
#define DA_ERR_INVALID_CLIENT		-102
#define DA_ERR_INVALID_DD		-103
#define DA_ERR_INVALID_URL		-104
#define DA_ERR_INVALID_HANDLE		-105
#define DA_ERR_INVALID_INSTALL_PATH	-106
#define DA_ERR_INVALID_MIME_TYPE	-107

// Client passed correct parameter, but Download Agent rejects the request because of internal policy.
#define DA_ERR_MISMATCH_CLIENT_DD	-150
#define DA_ERR_ALREADY_CANCELED		-160
#define DA_ERR_ALREADY_SUSPENDED	-161
#define DA_ERR_ALREADY_RESUMED		-162
#define DA_ERR_CANNOT_SUSPEND    	-170
#define DA_ERR_CANNOT_RESUME    	-171
#define DA_ERR_WAITING_USER_CONFIRM	-180
#define DA_ERR_WAITING_INSTALL_RESULT	-181
#define DA_ERR_INVALID_STATE		-190
#define DA_ERR_ALREADY_MAX_DOWNLOAD	-191
#define DA_ERR_UNSUPPORTED_PROTOCAL	-192
#define DA_ERR_CLIENT_IS_ALREADY_REGISTERED	-193
/**
 * @}
 */

/**
 * @{
 */
// System error (-200 ~ -299)
#define DA_ERR_FAIL_TO_MEMALLOC		-200
#define DA_ERR_FAIL_TO_CREATE_THREAD		-210
#define DA_ERR_FAIL_TO_OBTAIN_MUTEX		-220
#define DA_ERR_FAIL_TO_ACCESS_FILE	-230
#define DA_ERR_DISK_FULL  	-240
/**
 * @}
 */

/**
 * @{
 */
// Platform error (-300 ~ -399)
#define DA_ERR_FAIL_TO_GET_CONF_VALUE		-300
#define DA_ERR_FAIL_TO_ACCESS_STORAGE	-310
#define DA_ERR_DLOPEN_FAIL 		-330
/**
 * @}
 */

/**
 * @{
 */
// Network error (-400 ~ -499)
#define DA_ERR_NETWORK_FAIL		-400
#define DA_ERR_UNREACHABLE_SERVER		-410
#define DA_ERR_HTTP_TIMEOUT         	-420
#define DA_ERR_SSL_FAIL         	-430
/**
 * @}
 */

/**
 * @{
 */
// HTTP error - not conforming with HTTP spec (-500 ~ -599)
#define DA_ERR_MISMATCH_CONTENT_TYPE	-500
#define DA_ERR_MISMATCH_CONTENT_SIZE	-501
#define DA_ERR_SERVER_RESPOND_BUT_SEND_NO_CONTENT	-502
/**
 * @}
 */

/**
 * @{
 */
// install error (-800 ~ -899)
#define DA_ERR_FAIL_TO_INSTALL_FILE	-800
/**
*@}
*/

/**
*@}
*/
#endif

