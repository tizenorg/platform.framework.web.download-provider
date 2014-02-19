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

#ifndef _Download_Agent_Interface_H
#define _Download_Agent_Interface_H

#ifndef EXPORT_API
#define EXPORT_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C"
{
#endif

#include "download-agent-defs.h"
#include <stdarg.h>

/**
 * @struct user_paused_info_t
 * @brief Download Agent will send its state through this structure.
 * @see da_paused_info_cb
 * @par
 *   This is used only by callback /a user_paused_info_t. \n
 */
typedef struct {
	/// download request id for this notification
	int download_id;
} user_paused_info_t;

/**
 * @struct user_progress_info_t
 * @brief Download Agent will send current downloading file's information through this structure.
 * @see da_progress_info_cb
 * @par
 *   This is used only by callback /a da_progress_info_cb. \n
 */
typedef struct {
	/// download request id for this updated download information
	int download_id;
	/// received size of chunked data.
	unsigned long int received_size;
} user_progress_info_t;

/**
 * @struct user_download_info_t
 * @brief Download Agent will send current download's information through this structure.
 * @see da_started_info_cb
 * @par
 *   This is used only by callback /a da_started_info_cb. \n
 */
typedef struct {
	/// download request id for this updated download information
	int download_id;
	/// file's mime type from http header.
	char *file_type;
	/// file size from http header.
	unsigned long int file_size;
	/// This is temporary file path.
	char *tmp_saved_path;
	/// This is the file name for showing to user.
	char *content_name;
	/// etag string value for resume download,
	char *etag;
} user_download_info_t;

typedef struct {
	/// download request id for this updated download information
	int download_id;
	/// This has only file name for now.
	char *saved_path;
	/// etag string value for resume download,
	/// This is returned when the download is failed and the etag is received from content server
	char *etag;
	/// convey error code if necessary, or it is zero.
	int err;
	/// http status code if necessary, or it is zero.
	int http_status;
} user_finished_info_t;

typedef struct {
	const char **request_header;
	int request_header_count;
	const char *install_path;
	const char *file_name;
	const char *temp_file_path; /* For resume download, the "etag" value should be existed together */
	const char *etag; /* For resume download */
	const char *pkg_name; /* For system resource */
	void *user_data;
} extension_data_t;

/**
 * @typedef da_paused_cb
 * @brief Download Agent will call this function to paused its state.
 *
 * This is user callback function registered on \a da_init. \n
 *
 * @remarks For the most of time, this state is just informative, so, user doesn't need to do any action back to Download Agent.
 *
 * @warning Download will be holding until getting user confirmation result through the function.
 *
 * @param[in]		state		state from Download Agent
 * @param[in]		user_param	user parameter which is set with \a DA_FEATURE_USER_DATA
 *
 * @see da_init
 * @see da_client_cb_t
 */
typedef void (*da_paused_info_cb) (user_paused_info_t *paused_info, void *user_param);

/**
 * @brief Download Agent will call this function to update received size of download-requested file.
 *
 * This is user callback function registered on \a da_init. \n
 * This is informative, so, user doesn't need to do any action back to Download Agent.\n
 *
 * @param[in]		progress_info		updated downloading information
 * @param[in]		user_param	user parameter which is set with \a DA_FEATURE_USER_DATA
 *
 * @see da_init
 * @see da_client_cb_t
 */
typedef void (*da_progress_info_cb) (user_progress_info_t *progress_info, void *user_param);

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
typedef void (*da_started_info_cb) (user_download_info_t *download_info, void *user_param);

typedef void (*da_finished_info_cb) (user_finished_info_t *finished_info, void *user_param);
 /**
  * @struct da_client_cb_t
  * @brief This structure convey User's callback functions for \a da_init
  * @see da_init
  */
typedef struct {
	/// callback to convey download information
	da_started_info_cb update_dl_info_cb;
	/// callback to convey downloading information while downloading including received file size
	da_progress_info_cb update_progress_info_cb;
	/// callback to convey saved path
	da_finished_info_cb finished_info_cb;
	/// callback to convey etag value
	da_paused_info_cb paused_info_cb;
} da_client_cb_t;

/**
 * @fn int da_init (da_client_cb_t *da_client_callback)
 * @brief This function initiates Download Agent and registers user callback functions.
 * @warning This should be called at once when client application is initialized before using other Download Agent APIs
 * @warning This function is paired with da_deinit function.
 *
 * @pre None.
 * @post None.
 *
 * @param[in]	da_client_callback	User callback function structure. The type is struct data pointer.
 * @return		DA_RESULT_OK for success, or DA_ERR_XXX for fail. DA_ERR_XXX is defined at download-agent-def.h.
 * @remarks		User MUST call this function first rather than any other DA APIs. \n
 * 				Please do not call UI code at callback function in direct. \n
 * 				It is better that it returns as soon as copying the data of callback functon. \n
 * @see da_deinit
 * @par Example
 * @code
 * #include <download-agent-interface.h>
 *
 * void da_started_info_cb(user_download_info_t *download_info,void *user_param);
 * void da_progress_info_cb(user_downloading_info_t *downloading_info,void *user_param);
 * void da_finished_cb(user_finished_info_t *complted_info, void *user_param);
 * void da_paused_info_cb(user_paused_info_t *paused_info, void *user_param);
 *
 * int download_initialize()
 * {
 *	int da_ret;
 *	da_client_cb_t da_cb = {0};
 *
 *	da_cb.update_dl_info_cb = &update_download_info_cb;
 *	da_cb.update_progress_info_cb = &progress_info_cb;
 *	da_cb.finished_info_cb = &finished_info_cb;
 *	da_cb.paused_info_cb = &paused_cb;
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
EXPORT_API int da_init(da_client_cb_t *da_client_callback);

 /**
 * @fn int da_deinit ()
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
EXPORT_API int da_deinit();

 /**
 * @fn int da_start_download(const char *url, int *download_id)
 * @brief This function starts to download a content on passed URL.
 *
 * Useful information and result are conveyed through following callbacks.
 * @li da_started_info_cb
 * @li da_progress_cb
 *
 * @pre da_init() must be called in advance.
 * @post None.
 * @remarks
  * 	Downloaded file is automatically registered to system. (e.g. File DB) \n
 * 	If there is another file has same name on registering directory, new one's name would have numbering postfix. \n
 * 	(e.g. abc.mp3 to abc_1.mp3)
 *
 * @param[in]	url				url to start download
 * @param[out]	download_id		assigned download request id for this URL
 * @return		DA_RESULT_OK for success, or DA_ERR_XXX for fail. DA_ERR_XXX is defined at download-agent-def.h.
 *
 * @see None.
 *
 * @par Example
 * @code
 * #include <download-agent-interface.h>
 *
 * int da_ret;
 * int download_id;
 * char *url = "http://www.test.com/sample.mp3";
 *
 * da_ret = da_start_download(url,&download_id);
 * if (da_ret == DA_RESULT_OK)
 *	printf("download requesting is successed\n");
 * else
 *	printf("download requesting is failed with error code %d\n", da_ret);
 * @endcode
 */
EXPORT_API int da_start_download(const char *url, int *download_id);

/**
* @fn int da_start_download_with_extension(const char *url, extension_data_t ext_data, int *download_id)
* @brief This function starts to download a content on passed URL with passed extension.
*
* Useful information and result are conveyed through following callbacks.
* @li da_started_info_cb
* @li da_progress_cb
*
* @pre da_init() must be called in advance.
* @post None.
* @remarks This API operation is  exactly same with da_start_download(), except for input properties.	\n
*
* @param[in]	url	url to start download
* @param[in]	ext_data extension data
* @param[out]	download_id	assigned download request id for this URL
* @return	DA_RESULT_OK for success, or DA_ERR_XXX for fail. DA_ERR_XXX is defined at download-agent-def.h.
*
*
* @par Example
* @code
  #include <download-agent-interface.h>

	int da_ret;
	int download_id;
	extension_data_t ext_data = {0,};
	const char *url = "https://www.test.com/sample.mp3";
	const char *install_path = "/myFiles/music";
	const char *my_data = strdup("data");
	ext_data.install_path = install_path;
	ext_data.user_data = (void *)my_data;

	da_ret = da_start_download_with_extension(url, &download_id, &ext_data);
	if (da_ret == DA_RESULT_OK)
            printf("download requesting is successed\n");
	else
            printf("download requesting is failed with error code %d\n", da_ret);
  @endcode
*/
EXPORT_API int da_start_download_with_extension(const char *url,
	extension_data_t *ext_data,
	int *download_id
);


/**
 * @fn int da_cancel_download(int download_id)
 * @brief This function cancels a download for passed download_id.
 *
 * Client can use this function if user wants to cancel already requested download.
 *
 * @remarks Should check return value. \n
 * 			If return value is not DA_RESULT_OK, then previous requested download can be keep downloading.
 * @remarks After calling this function, all information for the download_id will be deleted. So, client cannot request anything for the download_id.
 *
 * @pre There should be exist ongoing or suspended download for download_id.
 * @post None.
 *
 * @param[in]		download_id		download request id
 * @return		DA_RESULT_OK for success, or DA_ERR_XXX for fail
 *
 * @see None.
 *
 * @par Example
 * @code
   #include <download-agent-interface.h>

   int da_ret;
   int download_id;

   da_ret = da_cancel_download(download_id);
   if(da_ret == DA_RESULT_OK) {
		// printf("download with [%d] is successfully canceled.\n", download_id);
   }
   else {
		// in this case, downloading with download_id is keep ongoing.
		printf("failed to cancel with error code %d\n", da_ret);
   }
 @endcode
 */
EXPORT_API int da_cancel_download(int download_id);


/**
 * @fn int da_suspend_download(int download_id)
 * @brief This function suspends downloading for passed download_id.
 *
 * Client can use this function if user wants to suspend already requested download.
 *
 * @remarks Should check return value. \n
 * 			If return value is not DA_RESULT_OK, then previous requested download can be keep downloading.
 * @remarks After calling this function, all information for the download_id will be remained. So, client can request resume for the download_id.
 * @remarks Client should cancel or resume for this download_id, or all information for the download_id will be leaved forever.
 *
 * @pre There should be exist ongoing download for download_id.
 * @post None.
 *
 * @param[in]		download_id		download request id
 * @return		DA_RESULT_OK for success, or DA_ERR_XXX for fail
 *
 * @see da_resume_download()
 * @see da_cancel_download()
 *
 * @par Example
 * @code
   #include <download-agent-interface.h>

   int da_ret;
   int download_id;

   da_ret = da_suspend_download(download_id);
   if(da_ret == DA_RESULT_OK) {
		// printf("download with [%d] is successfully suspended.\n", download_id);
   }
   else {
		// in this case, downloading with download_id is keep ongoing.
		printf("failed to suspend with error code %d\n", da_ret);
   }
 @endcode
 */
EXPORT_API int da_suspend_download(int download_id);

EXPORT_API int da_suspend_download_without_update(int download_id);
/**
 * @fn int da_resume_download(int download_id)
 * @brief This function resumes downloading for passed download_id.
 *
 * Client can use this function if user wants to resume suspended download.
 *
 * @remarks Should check return value. \n
 * 			If return value is not DA_RESULT_OK, then requested download can be not to resume.
 *
 * @pre There should be exist suspended download for download_id.
 * @post None.
 *
 * @param[in]		download_id		download request id
 * @return		DA_RESULT_OK for success, or DA_ERR_XXX for fail
 *
 * @see da_suspend_download()
 *
 * @par Example
 * @code
   #include <download-agent-interface.h>

   int da_ret;
   int download_id;

   da_ret = da_resume_download(download_id);
   if(da_ret == DA_RESULT_OK) {
		// printf("download with [%d] is successfully resumed.\n", download_id);
   }
   else {
		// in this case, downloading with download_id is keep suspended.
		printf("failed to resume with error code %d\n", da_ret);
   }
 @endcode
 */
EXPORT_API int da_resume_download(int download_id);

/**
 * @fn int da_is_valid_download_id(int download_id)
 * @brief This function return the download id is valid and the download thread is still alive.
 *
 * Client can use this function if user wants to resume download.
 * If the download id is vaild and the download thread is alive, it can resume download with using da_resume_download()
 * If the the download thread was already terminated due to restarting the process,
 *  it can resume download with using da_start_download_with_extension()
 *
 *
 *
 * @remarks Should check return value. \n
 * 			If return value is not DA_RESULT_OK, then requested download can be not to resume.
 *
 * @pre There should be exist suspended download for download_id.
 * @post None.
 *
 * @param[in]		download_id		download request id
 * @return		1 for success, or 0 for fail
  *
  */
EXPORT_API int da_is_valid_download_id(int download_id);

#ifdef __cplusplus
}
#endif

#endif //_Download_Agent_Interface_H


