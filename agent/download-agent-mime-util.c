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

#include <xdgmime.h>

#include "download-agent-debug.h"
#include "download-agent-mime-util.h"
#include "download-agent-pthread.h"

#define IS_PROHIBITED_CHAR(c)	((c) == ';' || (c) == '\\' || (c) == '/' || (c) == ':' || (c) == '*' || (c) == '?' || (c) == '"' || (c) == '>' || (c) == '<' || (c) == '|' || (c) == '(' || (c) == ')')
#define IS_SPACE_CHARACTER(c)	((c) == '\t')

#define MAX_EXT_TABLE_INDEX 16
Ext_translation_table ext_trans_table [MAX_EXT_TABLE_INDEX] = {
		{"*.xla",			"*.xls"},
		{"*.pot",			"*.ppt"},
		{"*.xsl",			"*.xml"},
		{"*.spl",			"*.swf"},
		{"*.oga",			"*.ogg"},
		{"*.jpe",			"*.jpg"},//5
		{"*.CSSL",			"*.css"},
		{"*.htm",			"*.html"},
		{"*.hxx",			"*.hpp"},
		{"*.c++",			"*.cpp"},
		{"CMakeLists.txt",	"*.cmake"},//10
		{"*.ime",			"*.imy"},
		{"Makefile",		"makefile"},
		{"*.3g2",			"*.3gp"},
		{"*.mp2",			"*.mpg"},
		{"*.divx",			"*.avi"},//15
	};
/* This is samsung mime policy
 * 1. if the mime is audio/m4a, the extension name is defined as "m4a" for launching music player
*/
#ifdef _SAMSUNG_MIME_POLICY
#define MAX_SEC_MIME_TABLE_INDEX 1
struct sec_mime_table_t {
	char *mime;
	char *ext;
};
struct sec_mime_table_t sec_mime_table [MAX_SEC_MIME_TABLE_INDEX] = {
	{"audio/m4a",		"m4a"},
};
#endif

const char *ambiguous_MIME_Type_list[] = {
		"text/plain",
		"application/octet-stream"
};

/* Because xdgmime is not thread safety, this mutex is necessary */
pthread_mutex_t mutex_for_xdgmime = PTHREAD_MUTEX_INITIALIZER;

da_bool_t is_ambiguous_MIME_Type(const char *in_mime_type)
{
	DA_LOG_FUNC_LOGV(Default);

	if (!in_mime_type)
		return DA_FALSE;

	int index = 0;
	int list_size = sizeof(ambiguous_MIME_Type_list) / sizeof(const char *);
	for (index = 0 ; index < list_size ; index++) {
		if (0 == strncmp(in_mime_type, ambiguous_MIME_Type_list[index],
				strlen(ambiguous_MIME_Type_list[index]))) {
			DA_SECURE_LOGD("It is ambiguous! [%s]", ambiguous_MIME_Type_list[index]);
			return DA_TRUE;
		}
	}

	return DA_FALSE;
}

da_result_t da_mime_get_ext_name(char *mime, char **ext)
{
	da_result_t ret = DA_RESULT_OK;
	const char **extlist = DA_NULL;
	const char *unaliased_mimetype = DA_NULL;
	char ext_temp[DA_MAX_STR_LEN] = {0,};
	char *temp = NULL;

	DA_LOG_FUNC_LOGV(Default);

	if (DA_NULL == mime || DA_NULL == ext) {
		ret = DA_ERR_INVALID_ARGUMENT;
		DA_LOG_ERR(Default,"Invalid mime type");
		goto ERR;
	}
//	DA_SECURE_LOGD("mime str[%s]ptr[%p]len[%d]",mime,mime,strlen(mime));
	/* unaliased_mimetype means representative mime among similar types */
	_da_thread_mutex_lock(&mutex_for_xdgmime);
	unaliased_mimetype = xdg_mime_unalias_mime_type(mime);
	_da_thread_mutex_unlock(&mutex_for_xdgmime);

	if (unaliased_mimetype == DA_NULL) {
		ret = DA_ERR_INVALID_MIME_TYPE;
		DA_LOG_ERR(Default,"Invalid mime type : No unsaliased mime type");
		goto ERR;
	}
	DA_SECURE_LOGD("unaliased_mimetype[%s]\n",unaliased_mimetype);

	/* Get extension name from shared-mime-info */
	_da_thread_mutex_lock(&mutex_for_xdgmime);
	extlist = xdg_mime_get_file_names_from_mime_type(unaliased_mimetype);
	_da_thread_mutex_unlock(&mutex_for_xdgmime);
	if (extlist == DA_NULL || *extlist == DA_NULL) {
		int i = 0;
		ret = DA_ERR_INVALID_MIME_TYPE;
		DA_LOG(Default,"No extension list");
#ifdef _SAMSUNG_MIME_POLICY
		for (i = 0; i < MAX_SEC_MIME_TABLE_INDEX; i++)
		{
			if (strncmp(sec_mime_table[i].mime, mime, strlen(mime)) == 0) {
				strncpy(ext_temp, sec_mime_table[i].ext, DA_MAX_STR_LEN-1);
				ret = DA_RESULT_OK;
				break;
			}
		}
#endif
	} else { /* For drm case, this else statement is needed */
		DA_SECURE_LOGD("extlist[%s]\n",*extlist);
		strncpy(ext_temp, *extlist, DA_MAX_STR_LEN);
		/* If only one extension name is existed, don't enter here */
		while (*extlist != NULL) {
			int i = 0;
			/* If there are existed many extension names,
			 *  try to search common extension name from table
			 *  with first mime type at extension list*/
			for (i = 0; i < MAX_EXT_TABLE_INDEX; i++)
			{
				if (strncmp(ext_trans_table[i].standard,*extlist,
						strlen(*extlist)) == 0) {
					memset(ext_temp, 0x00, DA_MAX_STR_LEN);
					strncpy(ext_temp,ext_trans_table[i].normal, DA_MAX_STR_LEN-1);
					break;
				}
			}
			DA_LOG_VERBOSE(Default,"index[%d]\n",i);
			/* If there is a mime at extension transform table */
			if (i < MAX_EXT_TABLE_INDEX) {
				break;
			}
			DA_SECURE_LOGD("extlist[%s]\n",*extlist);
			extlist++;
		}
//		DA_SECURE_LOGD("extension from shared mime info[%s]",ext_temp);
	}

	if (strlen(ext_temp) < 1) {
		/* If there is no mime string for OMA descriptor mime type */
		if (strncmp(DD_MIME_STR, mime, strlen(DD_MIME_STR)) == 0) {
			strncpy(ext_temp, DD_EXT_STR, DA_MAX_STR_LEN-1);
			ret = DA_RESULT_OK;
			/* If there is no extension name for "applicaion/vnd.oma.drm.messeages"
			 *  at shared-mime-info*/
		} else if (strncmp(DRM_MIME_MSG_STR, mime,
				strlen(DRM_MIME_MSG_STR)) == 0) {
			strncpy(ext_temp, DRM_EXT_STR, DA_MAX_STR_LEN-1);
			/* If there is extension name at extlist, the return value can have an error.*/
			ret = DA_RESULT_OK;
		} else {
			ret = DA_ERR_INVALID_MIME_TYPE;
			DA_LOG_ERR(Default,"Invalid mime type : no extension name at list");
		}
	}
	if (ret != DA_RESULT_OK)
		goto ERR;

	temp = strchr(ext_temp,'.');
	if (temp == NULL)
		temp = ext_temp;
	else
		temp++;

	*ext = (char*)calloc(1, strlen(temp) + 1);
	if (*ext != DA_NULL) {
		strncpy(*ext, temp,strlen(temp));
	} else	{
		ret = DA_ERR_FAIL_TO_MEMALLOC ;
		goto ERR ;
	}
ERR:
	return ret;
}

da_bool_t da_get_extension_name_from_url(char *url, char **ext)
{
	da_bool_t ret = DA_TRUE;
	char *buff = DA_NULL;
	char *temp_str = DA_NULL;
	int buf_len = 0;

	DA_LOG_FUNC_LOGV(Default);

	if (DA_NULL == url || DA_NULL == ext) {
		ret = DA_FALSE;
		DA_LOG_ERR(Default,"Invalid Argument");
		return ret;
	}

	if ((temp_str = strrchr(url,'/'))) {
		if ((buff = strrchr(temp_str,'.'))) {
			char *q = DA_NULL;
			buff++;
			/* check to exist "?" after extension name */
			q = strrchr(buff,'?');
			if (q) {
				buf_len = strlen(buff) - strlen(q);
			} else {
				buf_len = strlen(buff);
			}
			*ext = (char*) calloc(1, buf_len + 1) ;

			if (DA_NULL == *ext) {
				ret = DA_FALSE;
				DA_LOG_ERR(Default,"Memory Fail");
				goto ERR;
			}
			strncpy(*ext,buff,buf_len);
			DA_SECURE_LOGD("extention name[%s]",*ext);
			return ret;
		}
	}
ERR:
	if (*ext) {
		free(*ext);
		*ext = DA_NULL;
	}
	return ret;
}

/* FIXME move this function to another file */
da_bool_t da_get_file_name_from_url(char *url, char **name)
{
	da_bool_t ret = DA_TRUE;
	char *buff = DA_NULL;
	char *Start = NULL;
	char *End = NULL;
	char c = 0;
	int i = 0;
	int j = 0;
	int len_name = 0;
	char name_buff[DA_MAX_FILE_PATH_LEN] = {0,};

	DA_LOG_FUNC_LOGV(Default);

	if (DA_NULL == url || DA_NULL == name) {
		ret = DA_FALSE;
		DA_LOG_ERR(Default,"Invalid Argument");
		goto ERR;
	}
	*name = DA_NULL;
	if (!strstr(url, "http") && !strstr(url, "https")) {
		ret = DA_FALSE;
		DA_LOG_ERR(Default,"Invalid Argument");
		goto ERR;
    }

	buff = (char*) calloc(1, strlen(url) +1);
	if(DA_NULL == buff) {
		ret = DA_FALSE;
		DA_LOG_ERR(Default,"Memory Fail");
		goto ERR;
    }

	while((c = url[i++]) != 0) {
		if(c == '%') {
			char buffer[3] = {0,};
			buffer[0] = url[i++];
			buffer[1] = url[i++];
			buff[j++] = (char)strtol(buffer,NULL,16);
		} else {
			buff[j++] = c;
		}
	}
	End = strstr(buff, "?");
	if (DA_NULL != End) {
		Start = End -1;
		while(*(Start) != '/') {
			Start--;
		}
		if ((*(Start) == '/') && ((len_name = (End - Start)) > 1)) {
			Start++;
			if (DA_MAX_FILE_PATH_LEN <= len_name)	{
				strncpy(name_buff, Start, DA_MAX_FILE_PATH_LEN-1);
				name_buff[DA_MAX_FILE_PATH_LEN-1] = '\0';
			} else {
				strncpy(name_buff, Start, len_name);
				name_buff[len_name] = '\0';
			}
		} else {
			ret = DA_FALSE;
			goto ERR ; /*Name not found*/
		}
	} else {
		int urlLen = strlen (buff);
		int Start_pos = 0;
		Start_pos = urlLen - 1;

		while(Start_pos > 0) {
			if(buff[Start_pos] == '/')
				break;
			Start_pos--;
		}
		Start_pos++;
		if (Start_pos == 0 || urlLen - Start_pos <= 0) {
			ret = DA_FALSE;
			goto ERR;
		}
		while(Start_pos < urlLen) {
			name_buff[len_name++] = buff[Start_pos++];
			if (DA_MAX_FILE_PATH_LEN <= len_name) {
				name_buff[DA_MAX_FILE_PATH_LEN-1] ='\0';
				break;
			}
		}
	}

	if (len_name) {
		End = strrchr(name_buff, '.');
		if (End != NULL) {
			*End = '\0';
		}
//		DA_SECURE_LOGD("file name BEFORE removing prohibited character = %s", name_buff);
		delete_prohibited_char(name_buff, strlen(name_buff));
//		DA_SECURE_LOGD("file name AFTER removing prohibited character = %s", name_buff);
		len_name = strlen(name_buff);
		*name = (char*) calloc(1, len_name + 1);
		if (*name) {
			strncpy(*name, name_buff,len_name);
		}
	}
//	DA_SECURE_LOGD("Extracted file name : %s", *name);
ERR:
	if (buff) {
		free (buff);
		buff = DA_NULL;
    }
	return ret;
}

void delete_prohibited_char(char *szTarget, int str_len)
{
	char *chk_str = NULL;
	int i = 0;
	int j = 0;
	int tar_len = 0;

	if(szTarget == NULL || str_len <= 0 || strlen(szTarget) != str_len) {
		DA_LOG_ERR(Default,"Invaild Parameter\n");
		return;
	}

	chk_str = (char *)calloc(1, str_len + 1);
	if(chk_str == NULL)
		return;

	while(szTarget[j] != '\0') {
		if(IS_PROHIBITED_CHAR(szTarget[j]) == DA_FALSE &&
					IS_SPACE_CHARACTER(szTarget[j]) == DA_FALSE) {
			chk_str[i] = szTarget[j];
			i++;
		}
		j++;
	}

	chk_str[i] = '\0';
	tar_len = strlen(chk_str);

	if(tar_len <= 0)
		szTarget[0] = '\0';
	else {
		for(i = 0; i < tar_len; i++)
		{
			szTarget[i] = chk_str[i];
		}
		szTarget[i] = '\0';
	}

	if(chk_str != NULL)	{
		free(chk_str);
	}
	return;
}

