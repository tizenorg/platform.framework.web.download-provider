/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd All Rights Reserved
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

#include <stdlib.h>
#include <errno.h>
#include <sys/statfs.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <sys/smack.h>

#include <download-provider.h>
#include <download-provider-log.h>
#include <download-provider-utils.h>

#define SMACKFS_MAGIC 0x43415d53
#define SMACKFS_MNT "/smack"

static int __dp_smack_is_transmute(char *path)
{
	char *dir_label = NULL;
	int ret = -1;
	if (smack_getlabel(path, &dir_label, SMACK_LABEL_TRANSMUTE) == 0 &&
			dir_label != NULL) {
		if (strncmp(dir_label, "TRUE", strlen(dir_label)) == 0)
			ret = 0;
	}
	free(dir_label);
	return ret;
}

int dp_smack_is_mounted()
{
	struct statfs sfs;
	int ret;
	do {
		ret = statfs(SMACKFS_MNT, &sfs);
	} while (ret < 0 && errno == EINTR);
	if (ret) {
		TRACE_ERROR("[SMACK ERROR]");
		return -1;
	}
	if (sfs.f_type == SMACKFS_MAGIC) {
		return 1;
	}
	TRACE_ERROR("[SMACK DISABLE]");
	return 0;
}

int dp_smack_set_label(char *label, char *source, char *target)
{
	if (label == NULL || source == NULL || target == NULL)
		return DP_ERROR_PERMISSION_DENIED;

	int is_setted_dir_label = 0;
	int errorcode = DP_ERROR_NONE;

	if (__dp_smack_is_transmute(source) < 0) {
		TRACE_SECURE_ERROR("[SMACK] no transmute:%s", source);
	} else {
		char *dir_label = NULL;
		if (smack_getlabel(source, &dir_label, SMACK_LABEL_ACCESS) == 0) {
			if (smack_have_access(label, dir_label, "t") > 0) {
				if (smack_setlabel(target, dir_label, SMACK_LABEL_ACCESS) != 0) {
					TRACE_SECURE_ERROR("[SMACK ERROR] label:%s", dir_label);
					errorcode = DP_ERROR_PERMISSION_DENIED;
				} else {
					is_setted_dir_label = 1;
				}
			} else {
				TRACE_SECURE_ERROR("[SMACK ERROR] access:%s/%s", label, dir_label);
				errorcode = DP_ERROR_PERMISSION_DENIED;
			}
		} else {
			TRACE_SECURE_ERROR("[SMACK ERROR] no label:", source);
			errorcode = DP_ERROR_PERMISSION_DENIED;
		}
		free(dir_label);
	}
	if (is_setted_dir_label == 0 &&
			smack_setlabel(target, label, SMACK_LABEL_ACCESS) != 0) {
		TRACE_SECURE_ERROR("[SMACK ERROR] label:%s", label);
		errorcode = DP_ERROR_PERMISSION_DENIED;
		// remove file.
		if (dp_is_file_exist(target) == 0)
			unlink(target);
	}
	return errorcode;
}

char *dp_smack_get_label_from_socket(int sock)
{
	char *label = NULL;
	if (smack_new_label_from_socket(sock, &label) != 0) {
		free(label);
		return NULL;
	}
	return label;
}

int dp_smack_is_valid_dir(int uid, int gid, char *smack_label, char *dir)
{
	if (smack_label == NULL || dir == NULL) {
		TRACE_ERROR("check parameter %s/%s", smack_label, dir);
		return -1;
	}
	int ret = -1;
	struct stat dstate;
	if (stat(dir, &dstate) == 0) {
		if ((dstate.st_uid == uid && (dstate.st_mode & (S_IRUSR | S_IWUSR)) == (S_IRUSR | S_IWUSR)) ||
				(dstate.st_gid == gid && (dstate.st_mode & (S_IRGRP | S_IWGRP)) == (S_IRGRP | S_IWGRP)) ||
				((dstate.st_mode & (S_IROTH | S_IWOTH)) == (S_IROTH | S_IWOTH))) {
			char *dir_label = NULL;
			if (smack_getlabel(dir, &dir_label, SMACK_LABEL_ACCESS) == 0 &&
					smack_have_access(smack_label, dir_label, "rw") > 0) {
				ret = 0;
			}
			free(dir_label);
		}
	}
	return ret;
}

int dp_is_valid_dir(const char *dirpath)
{
	struct stat dir_state;
	int stat_ret;
	if (dirpath == NULL) {
		TRACE_ERROR("check path");
		return -1;
	}
	stat_ret = stat(dirpath, &dir_state);
	if (stat_ret == 0 && S_ISDIR(dir_state.st_mode)) {
		return 0;
	}
	return -1;
}

void dp_rebuild_dir(const char *dirpath, mode_t mode)
{
	if (dp_is_valid_dir(dirpath) < 0) {
		if (mkdir(dirpath, mode) == 0) {
			TRACE_INFO("check directory:%s", dirpath);
			if (smack_setlabel(dirpath, "_", SMACK_LABEL_ACCESS) != 0) {
				TRACE_SECURE_ERROR("failed to set smack label:%s", dirpath);
			}
		} else {
			TRACE_STRERROR("failed to create directory:%s", dirpath);
		}
	}
}
