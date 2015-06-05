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

#ifndef _Download_Agent_Encoding_H
#define _Download_Agent_Encoding_H

#include "download-agent-type.h"

da_bool_t is_base64_encoded_word(const char *in_str);
da_ret_t decode_base64_encoded_str(const char *in_encoded_str,
	char **out_decoded_ascii_str);
void decode_url_encoded_str(const char *in_encoded_str, char **out_str);

#endif // _Download_Agent_Encoding_H
