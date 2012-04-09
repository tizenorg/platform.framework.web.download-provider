/*
 * Copyright (c) 2000 - 2012 Samsung Electronics Co., Ltd All Rights Reserved 
 *
 * This file is part of Download Provider
 * Written by Jungki Kwak <jungki.kwak@samsung.com>
 *
 * PROPRIETARY/CONFIDENTIAL
 *
 * This software is the confidential and proprietary information of SAMSUNG ELECTRONICS
 * ("Confidential Information"). You shall not disclose such Confidential Information
 * and shall use it only in accordance with the terms of the license agreement
 * you entered into with SAMSUNG ELECTRONICS.
 *
 * SAMSUNG make no representations or warranties about the suitability of the software,
 * either express or implied, including but not limited to the implied warranties of merchantability,
 * fitness for a particular purpose, or non-infringement.
 *
 * SAMSUNG shall not be liable for any damages suffered by licensee as a result of using,
 * modifying or distributing this software or its derivatives.
 */
/**
 * @file 	download-provider-common.h
 * @author	Jungki Kwak (jungki.kwak@samsung.com)
 * @brief	Common define and data
 */

#ifndef DOWNLOAD_PROVIDER_DOWNLOAD_COMMON_H
#define DOWNLOAD_PROVIDER_DOWNLOAD_COMMON_H

#include <libintl.h>
#include "download-provider-debug.h"

#if !defined(PACKAGE)
	#define PACKAGE "download-provider"
#endif

#define _EDJ(o) elm_layout_edje_get(o)
#define __(s) dgettext(PACKAGE, s)
#define S_(s) dgettext("sys_string", s)

#define ERROR_POPUP_LOW_MEM S_("IDS_COM_POP_NOT_ENOUGH_MEMORY")
#define ERROR_POPUP_UNKNOWN S_("IDS_COM_POP_INTERNAL_ERROR")
#define ERROR_POPUP_INVALID_URL S_("IDS_COM_POP_INVALID_URL")

#define DP_DRM_ICON_PATH IMAGEDIR"/U06_icon_DRM.png"
#define DP_JAVA_ICON_PATH IMAGEDIR"/U06_icon_Java.png"
#define DP_UNKNOWN_ICON_PATH IMAGEDIR"/U06_icon_Unknown.png"
#define DP_EXCEL_ICON_PATH IMAGEDIR"/U06_icon_excel.png"
#define DP_HTML_ICON_PATH IMAGEDIR"/U06_icon_html.png"
#define DP_MUSIC_ICON_PATH IMAGEDIR"/U06_icon_music.png"
#define DP_PDF_ICON_PATH IMAGEDIR"/U06_icon_pdf.png"
#define DP_PPT_ICON_PATH IMAGEDIR"/U06_icon_ppt.png"
#define DP_RINGTONE_ICON_PATH IMAGEDIR"/U06_icon_ringtone.png"
#define DP_TEXT_ICON_PATH IMAGEDIR"/U06_icon_text.png"
#define DP_WORD_ICON_PATH IMAGEDIR"/U06_icon_word.png"
#define DP_VIDEO_ICON_PATH IMAGEDIR"/U06_icon_video.png"
#define DP_IMAGE_ICON_PATH IMAGEDIR"/U06_icon_image.png"

#define MAX_FILE_PATH_LEN 256
#define MAX_BUF_LEN 256

#define LOAD_HISTORY_COUNT 500

enum
{
	DP_CONTENT_NONE = 0,
	DP_CONTENT_IMAGE,
	DP_CONTENT_VIDEO,
	DP_CONTENT_MUSIC,
	DP_CONTENT_PDF,
	DP_CONTENT_WORD,
	DP_CONTENT_PPT, // 5
	DP_CONTENT_EXCEL,
	DP_CONTENT_HTML,
	DP_CONTENT_TEXT,
	DP_CONTENT_RINGTONE,
	DP_CONTENT_DRM, // 10
	DP_CONTENT_JAVA,
	DP_CONTENT_SVG,
	DP_CONTENT_FLASH,
	DP_CONTENT_UNKOWN
};

namespace DL_TYPE{
enum TYPE {
	TYPE_NONE,
	HTTP_DOWNLOAD,
	OMA_DOWNLOAD,
	MIDP_DOWNLOAD
};
}

namespace ERROR {
enum CODE {
	NONE,
	INVALID_URL,
	NETWORK_FAIL,
	NOT_ENOUGH_MEMORY,
	FAIL_TO_INSTALL,
	FAIL_TO_PARSE_DESCRIPTOR,
	OMA_POPUP_TIME_OUT,
	ENGINE_FAIL,
	UNKNOWN
};
}

#endif /* DOWNLOAD_PROVIDER_DOWNLOAD_COMMON_H */
