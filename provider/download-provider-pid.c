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

#include <unistd.h>
#include <fcntl.h>

//////////////////////////////////////////////////////////////////////////
/// @brief check whether daemon is alive
/// @warning lockfd should be managed without close()
/// @param the patch for locking the file
int dp_lock_pid(char *path)
{
	int lockfd = -1;
	if ((lockfd = open(path, O_WRONLY | O_CREAT, (0666 & (~000)))) < 0) {
		return -1;
	} else if (lockf(lockfd, F_TLOCK, 0) < 0) {
		close(lockfd);
		return -1;
	}
	return lockfd;
}
