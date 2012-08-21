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
 * @file	download-agent-interface.h
 * @brief	Interface  for Download Agent.
 * @author	Keunsoon Lee (keunsoon.lee@samsung.com)
 * @author	Jungki Kwak(jungki.kwak@samsung.com)
 *
 */

#ifndef _Download_Agent_Interface_H
#define _Download_Agent_Interface_H

#ifdef __cplusplus
 extern "C"
 {
#endif

#include "download-agent-defs.h"
#include <stdarg.h>

/**
  * @ingroup Internet_FW
  * @addtogroup DownloadAgent Download Agent
  * @{
  * @brief Download Agent is a UI-less shared library to download files from Internet.
  * @par Details
  *  Download Agent is a module to download stuffs through HTTP. \n
  *  Download Agent can download several files simultaneously using multi-thread. \n\n
  *  If any application wants to download something from Internet, then use this like following. \n
  *	 1) Be a client for Download Agent. \n
  *	 2) Pass a URL which you want to download to Download Agent. \n
  *	 3) Get notifications and result from Download Agent. \n
  */

/**
 * @ingroup DownloadAgent
 * @addtogroup QuickGuide Quick Guide
 * @{
 * @par Step 1. To implement required callbacks to receive notifications
 * 	\c da_notify_cb and \c da_update_download_info_cb are required almost all cases. \n
 * @remark Do not free anything on the callbacks. DA will take care of them.
 * @remark If you want to use \a user_param, set it with \a DA_FEATURE_USER_DATA
 * @remark You can free user_param when receiving one of following \a da_state.
 * 	@li DA_STATE_FINISHED
 * 	@li DA_STATE_FAILED
 * 	@li DA_STATE_CANCELED
 * @par
 * @code
 * // main.c
 * #include <download-agent-interface.h>
 *
 * void notify_cb(user_notify_info_t *notify_info, void* user_param)
 * {
 * 	if (!notify_info)
 * 		return;
 * 	printf("id [%d], state [%d]", notify_info->da_dl_req_id, notify_info->state);
 *	if (notify_info->state == DA_STATE_FAILED)
 *		printf("error code [%d]", notify_info->err);
 * }
 *
 * void update_download_info_cb(user_download_info_t *download_info, void* user_param)
 * {
 * 	if (!download_info)
 *		return;
 *	printf("id [%d], file size [%d/%d]", download_info->da_dl_req_id,
 *		download_info->file_size);
 * }
 *
 * void update_downloading_info_cb(user_downloading_info_t *downloading_info, void* user_param)
 * {
 * 	if (!downloading_info)
 *		return;
 *	printf("id [%d], received size [%d]", downloading_info->da_dl_req_id,
 *		downloading_info->total_received_size);
 * }
 * @endcode
 * @endcode
 *
 * @par Step 2. To register the callbacks
 *	set NULL for callbacks you do not want to know.
 * @code
 * // continued after Step 1
 *
 * void download_initialize()
 * {
 * 	int da_ret;
 * 	da_client_cb_t da_cb = { notify_cb, NULL, update_download_info_cb, update_downloading_info_cb, NULL };
 *
 * 	da_ret = da_init (&da_cb, 0);
 * 	if(da_ret == DA_RESULT_OK)
 * 		printf("successed\n");
 * 	else
 * 		printf("failed with error code %d\n", da_ret);
 * }
 * @endcode
 *
 * @par Step 3. To request a download
 * 	You can meet the da_req_id on Step 1 callbacks again. \n
 * 	Downloaded file is automatically registered to system. (e.g. File DB) \n
 * 	If there is another file has same name on registering directory, new one's name would have numbering postfix. \n
 * 	(e.g. abc.mp3 to abc_1.mp3)
 * @see da_start_download_with_extension
 * @code
 * // continued after Step 2
 * void start_download()
 * {
 * 	int da_ret;
 * 	da_handle_t da_req_id;
 *	char *url = "http://www.test.com/sample.mp3";
 * 	da_ret = da_start_download(url, &da_req_id);
  * 	if(da_ret == DA_RESULT_OK)
 * 		printf("successed\n");
 * 	else
 * 		printf("failed with error code %d\n", da_ret);
 * }
 * @endcode
 *
 * @par Step 4. To do whatever client wants with information obtained from callbacks
 *
 * @par Step 5. To de-initialize when client does not use Download Agent anymore
 * @code
 * // continued after Step 3
 * void download_deinit()
 * {
 * 	int da_ret;
 * 	da_ret = da_deinit();
   * 	if(da_ret == DA_RESULT_OK)
 * 		printf("successed\n");
 * 	else
 * 		printf("failed with error code %d\n", da_ret);
 * }
 * @endcode
 * @}
 */

 /**
  * @ingroup DownloadAgent
  * @addtogroup Reference
  * @{
  */

/**
 * @ingroup Reference
 * @addtogroup NotificationCallbacks Notification callbacks for client
 * @brief Download Agent notifies many information with these callbacks to client.
 * @{
 */
/**
 * @struct user_notify_info_t
 * @brief Download Agent will send its state through this structure.
 * @see da_notify_cb
 * @par
 *   This is used only by callback /a user_notify_info_t. \n
 */
typedef struct {
	/// download request id for this notification
	da_handle_t da_dl_req_id;
	/// Download Agent's current state corresponding to the da_dl_req_id
	da_state state;
	/// convey error code if necessary, or it is zero.
	int err;
} user_notify_info_t;

/**
 * @struct user_downloading_info_t
 * @brief Download Agent will send current downloading file's information through this structure.
 * @see da_update_downloading_info_cb
 * @par
 *   This is used only by callback /a da_update_downloading_info_cb. \n
 */
typedef struct {
	/// download request id for this updated download information
	da_handle_t da_dl_req_id;
	/// received size of chunked data.
	unsigned long int total_received_size;
	/// This has only file name for now.
	char *saved_path;
} user_downloading_info_t;

/**
 * @struct user_download_info_t
 * @brief Download Agent will send current download's information through this structure.
 * @see da_update_download_info_cb
 * @par
 *   This is used only by callback /a da_update_download_info_cb. \n
 */
typedef struct {
	/// download request id for this updated download information
	da_handle_t da_dl_req_id;
	/// file's mime type from http header.
	char *file_type;
	/// file size from http header.
	unsigned long int file_size;
	/// This is same with 'saved_path' for now.
	char *tmp_saved_path;
	/// This is raw data of response header
	char *http_response_header;
	/// This is raw data of chunked data
	char *http_chunked_data;
} user_download_info_t;

/**
 * @typedef da_notify_cb
 * @brief Download Agent will call this function to notify its state.
 *
 * This is user callback function registered on \a da_init. \n
 *
 * @remarks For the most of time, this state is just informative, so, user doesn't need to do any action back to Download Agent.
 * @remarks But, Download Agent will wait for user's specific action after notifying some states.
 *
 * @warning Download will be holding until getting user confirmation result through the function.
 *
 * @param[in]		state		state from Download Agent
 * @param[in]		user_param	user parameter which is set with \a DA_FEATURE_USER_DATA
 *
 * @see da_init
 * @see da_client_cb_t
 */
typedef void (*da_notify_cb) (user_notify_info_t *notify_info, void* user_param);

/**
 * @brief Download Agent will call this function to update received size of download-requested file.
 *
 * This is user callback function registered on \a da_init. \n
 * This is informative, so, user doesn't need to do any action back to Download Agent.\n
 *
 * @param[in]		downloading_info		updated downloading information
 * @param[in]		user_param	user parameter which is set with \a DA_FEATURE_USER_DATA
 *
 * @see da_init
 * @see da_client_cb_t
 */
typedef void (*da_update_downloading_info_cb) (user_downloading_info_t *downloading_info, void* user_param);

/**
 * @brief Download Agent will call this function to update mime type, temp file name, total file sizeand installed path.
 *
 * This is user callback function registered on \a da_init. \n
 * This is informative, so, user doesn't need to do any action back to Download Agent.\n
 *
 * @param[in]		download_info		updated download information
 * @param[in]		user_param	user parameter which is set with \a DA_FEATURE_USER_DATA
 *
 * @see da_init
 * @see da_client_cb_t
 */
typedef void (*da_update_download_info_cb) (user_download_info_t *download_info, void* user_param);

 /**
  * @struct da_client_cb_t
  * @brief This structure convey User's callback functions for \a da_init
  * @see da_init
  */
typedef struct {
	/// callback to convey \a da_state and error code
	da_notify_cb user_noti_cb;
	/// callback to convey download information
	da_update_download_info_cb update_dl_info_cb;
	/// callback to convey downloading information while downloading including received file size
	da_update_downloading_info_cb update_progress_info_cb;
} da_client_cb_t;
/**
 * @}
 */

/**
 * @fn int da_init (da_client_cb_t *da_client_callback,da_download_managing_method download_method)
 * @ingroup Reference
 * @brief This function initiates Download Agent and registers user callback functions.
 * @warning This should be called at once when client application is initialized before using other Download Agent APIs
 * @warning This function is paired with da_deinit function.
 *
 * @pre None.
 * @post None.
 *
 * @param[in]	da_client_callback	User callback function structure. The type is struct data pointer.
 * @param[in]	download_method			Application can choose to manage download manually or automatically.The Enum data da_download_managing_method is defined at download-agent-defs.h.
 * @return		DA_RESULT_OK for success, or DA_ERR_XXX for fail. DA_ERR_XXX is defined at download-agent-def.h.
 * @remarks		User MUST call this function first rather than any other DA APIs. \n
 * 				Please do not call UI code at callback function in direct. \n
 * 				It is better that it returns as soon as copying the data of callback functon. \n
 * @see da_deinit
 * @par Example
 * @code
 * #include <download-agent-interface.h>
 *
 * void da_notify_cb(user_notify_info_t *notify_info, void *user_param);
 * void da_update_download_info_cb(user_download_info_t *download_info,void* user_param);
 * void da_update_downloading_info_cb(user_downloading_info_t *downloading_info,void* user_param);
 *
 *
 * int download_initialize()
 * {
 *	int da_ret;
 *	da_client_cb_t da_cb = {0};
 *
 *	da_cb.user_noti_cb = &notify_cb;
 *	da_cb.update_dl_info_cb = &update_download_info_cb;
 *	da_cb.update_progress_info_cb = &update_downloading_info_cb;
 *
 *	da_ret = da_init (&da_cb, 0);
 *	if (da_ret == DA_RESULT_OK) {
 *		// printf("successed\n");
 *		return true;
 *	} else {
 *		// printf("failed with error code %d\n", da_ret);
 *		return fail;
 *	}
 * }
 * @endcode
 */
int da_init (
	da_client_cb_t *da_client_callback,
	da_download_managing_method download_method
);


 /**
 * @fn int da_deinit ()
 * @ingroup Reference
 * @brief This function deinitiates Download Agent.
 *
 * This function destroys all infomation for client manager.
 * When Download Agent is not used any more, please call this function.
 * Usually when client application is destructed, this is needed.
 *
 * @remarks This is paired with da_init. \n
 * 			The client Id should be the one from /a da_init(). \n
 * 			Otherwise, it cannot excute to deinitialize. \n
 *
 * @pre da_init() must be called in advance.
 * @post None.
 *
 * @return		DA_RESULT_OK for success, or DA_ERR_XXX for fail. DA_ERR_XXX is defined at download-agent-def.h.
 * @see da_init
 * @par Example
 * @code
 * #include <download-agent-interface.h>
 *
 *
 * int download_deinitialize()
 * {
 *	int da_ret;
 *	da_ret = da_deinit();
 *	if(da_ret == DA_RESULT_OK) {
 *		// printf("successed\n");
 *		return true;
 *	} else {
 *		// printf("failed with error code %d\n", da_ret);
 *		return fail;
 *	}
 * }
  @endcode
 */
int da_deinit ();


 /**
 * @fn int da_start_download(const char *url, da_handle_t *da_dl_req_id)
 * @ingroup Reference
 * @brief This function starts to download a content on passed URL.
 *
 * Useful information and result are conveyed through following callbacks.
 * @li da_notify_cb
 * @li da_update_download_info_cb
 *
 * @pre da_init() must be called in advance.
 * @post None.
 * @remarks
  * 	Downloaded file is automatically registered to system. (e.g. File DB) \n
 * 	If there is another file has same name on registering directory, new one's name would have numbering postfix. \n
 * 	(e.g. abc.mp3 to abc_1.mp3)
 *
 * @param[in]	url				url to start download
 * @param[out]	da_dl_req_id		assigned download request id for this URL
 * @return		DA_RESULT_OK for success, or DA_ERR_XXX for fail. DA_ERR_XXX is defined at download-agent-def.h.
 *
 * @see None.
 *
 * @par Example
 * @code
 * #include <download-agent-interface.h>
 *
 * int da_ret;
 * int da_dl_req_id;
 * char* url = "http://www.test.com/sample.mp3";
 *
 * da_ret = da_start_download(url,&da_dl_req_id);
 * if (da_ret == DA_RESULT_OK)
 *	printf("download requesting is successed\n");
 * else
 *	printf("download requesting is failed with error code %d\n", da_ret);
 * @endcode
 */
int da_start_download(
	const char *url,
	da_handle_t *da_dl_req_id
);

/**
* @fn int da_start_download_with_extension(const char *url, da_handle_t *da_dl_req_id, ...)
* @ingroup Reference
* @brief This function starts to download a content on passed URL with passed extension.
*
* Useful information and result are conveyed through following callbacks.
* @li da_notify_cb
* @li da_update_download_info_cb
*
* @pre da_init() must be called in advance.
* @post None.
* @remarks This API operation is  exactly same with da_start_download(), except for input properties.	\n
* 	Input properties' count is unlimited, but they MUST be a pair(name and value) and terminated by NULL.	\n
* 	Refer to following for property name and value type.	\n
*
* 	@li DA_FEATURE_REQUEST_HEADER : char** int * \n
* 	@li DA_FEATURE_USER_DATA	: void*	\n
* 	@li DA_FEATURE_INSTALL_PATH	: char*	\n
* 	@li DA_FEATURE_FILE_NAME	: char*	\n
*
* @see ExtensionFeatures
*
* @param[in]	url	url to start download
* @param[out]	da_dl_req_id	assigned download request id for this URL
* @param[in]	first_property_name	first property name to supply
* @param[in]	first_property_value	first property value to supply. This value's type can be int, char* or void*, which is up to first_property_name.
* @param[in]	...
* @return	DA_RESULT_OK for success, or DA_ERR_XXX for fail. DA_ERR_XXX is defined at download-agent-def.h.
*
*
* @par Example
* @code
  #include <download-agent-interface.h>

	int da_ret;
	int da_dl_req_id;
	const char *url = "https://www.test.com/sample.mp3";
	const char *install_path = "/myFiles/music";
	const char *my_data = strdup("data");	// should free after getting DA_STATE_FINISHED or DA_STATE_FAILED or DA_STATE_CANCELED

	da_ret = da_start_download_with_extension(url, &da_dl_req_id, DA_FEATURE_INSTALL_PATH, install_path
		, DA_FEATURE_USER_DATA, (void*)my_data, NULL);
	if (da_ret == DA_RESULT_OK)
            printf("download requesting is successed\n");
	else
            printf("download requesting is failed with error code %d\n", da_ret);
  @endcode
*/
int da_start_download_with_extension(
	const char    *url,
	da_handle_t    *da_dl_req_id,
	...
);


/**
 * @fn int da_cancel_download(da_handle_t da_dl_req_id)
 * @ingroup Reference
 * @brief This function cancels a download for passed da_dl_req_id.
 *
 * Client can use this function if user wants to cancel already requested download.
 *
 * @remarks Should check return value. \n
 * 			If return value is not DA_RESULT_OK, then previous requested download can be keep downloading.
 * @remarks After calling this function, all information for the da_dl_req_id will be deleted. So, client cannot request anything for the da_dl_req_id.
 *
 * @pre There should be exist ongoing or suspended download for da_dl_req_id.
 * @post None.
 *
 * @param[in]		da_dl_req_id		download request id
 * @return		DA_RESULT_OK for success, or DA_ERR_XXX for fail
 *
 * @see None.
 *
 * @par Example
 * @code
   #include <download-agent-interface.h>

   int da_ret;
   int da_dl_req_id;

   da_ret = da_cancel_download(da_dl_req_id);
   if(da_ret == DA_RESULT_OK) {
		// printf("download with [%d] is successfully canceled.\n", da_dl_req_id);
   }
   else {
		// in this case, downloading with da_dl_req_id is keep ongoing.
		printf("failed to cancel with error code %d\n", da_ret);
   }
 @endcode
 */
int da_cancel_download(
	da_handle_t	da_dl_req_id
);


/**
 * @fn int da_suspend_download(da_handle_t da_dl_req_id)
 * @ingroup Reference
 * @brief This function suspends downloading for passed da_dl_req_id.
 *
 * Client can use this function if user wants to suspend already requested download.
 *
 * @remarks Should check return value. \n
 * 			If return value is not DA_RESULT_OK, then previous requested download can be keep downloading.
 * @remarks After calling this function, all information for the da_dl_req_id will be remained. So, client can request resume for the da_dl_req_id.
 * @remarks Client should cancel or resume for this da_dl_req_id, or all information for the da_dl_req_id will be leaved forever.
 *
 * @pre There should be exist ongoing download for da_dl_req_id.
 * @post None.
 *
 * @param[in]		da_dl_req_id		download request id
 * @return		DA_RESULT_OK for success, or DA_ERR_XXX for fail
 *
 * @see da_resume_download()
 * @see da_cancel_download()
 *
 * @par Example
 * @code
   #include <download-agent-interface.h>

   int da_ret;
   int da_dl_req_id;

   da_ret = da_suspend_download(da_dl_req_id);
   if(da_ret == DA_RESULT_OK) {
		// printf("download with [%d] is successfully suspended.\n", da_dl_req_id);
   }
   else {
		// in this case, downloading with da_dl_req_id is keep ongoing.
		printf("failed to suspend with error code %d\n", da_ret);
   }
 @endcode
 */
int da_suspend_download(
	da_handle_t	da_dl_req_id
);



/**
 * @fn int da_resume_download(da_handle_t da_dl_req_id)
 * @ingroup Reference
 * @brief This function resumes downloading for passed da_dl_req_id.
 *
 * Client can use this function if user wants to resume suspended download.
 *
 * @remarks Should check return value. \n
 * 			If return value is not DA_RESULT_OK, then requested download can be not to resume.
 *
 * @pre There should be exist suspended download for da_dl_req_id.
 * @post None.
 *
 * @param[in]		da_dl_req_id		download request id
 * @return		DA_RESULT_OK for success, or DA_ERR_XXX for fail
 *
 * @see da_suspend_download()
 *
 * @par Example
 * @code
   #include <download-agent-interface.h>

   int da_ret;
   int da_dl_req_id;

   da_ret = da_resume_download(da_dl_req_id);
   if(da_ret == DA_RESULT_OK) {
		// printf("download with [%d] is successfully resumed.\n", da_dl_req_id);
   }
   else {
		// in this case, downloading with da_dl_req_id is keep suspended.
		printf("failed to resume with error code %d\n", da_ret);
   }
 @endcode
 */
int da_resume_download(
	da_handle_t	da_dl_req_id
);


/**
* @}
*/

/**
*@}
*/

#ifdef __cplusplus
  }
#endif

#endif //_Download_Agent_Interface_H


