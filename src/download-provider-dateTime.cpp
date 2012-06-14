/*
 * Copyright 2012  Samsung Electronics Co., Ltd
 *
 * Licensed under the Flora License, Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.tizenopensource.org/license
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * @file 	download-provider-dateTime.cpp
 * @author	Jungki Kwak (jungki.kwak@samsung.com)
 * @brief	data and utility APIs for Date and Time
 */

#include "runtime_info.h"
#include "download-provider-dateTime.h"

#define MAX_SKELETON_BUFFER_LEN 8
#define MAX_PATTERN_BUFFER_LEN 128

DateGroup::DateGroup()
	: count(0)
	, type(DATETIME::DATE_TYPE_NONE)
	, m_glGroupItem(NULL)
{
}

DateGroup::~DateGroup()
{
}

void DateGroup::initData()
{
	count = 0;
	m_glGroupItem = NULL;
}

DateUtil::DateUtil()
	: m_todayStandardTime(0)
	, dateShortFormat(NULL)
	, dateMediumFormat(NULL)
	, dateFullFormat(NULL)
	, timeFormat12H(NULL)
	, timeFormat24H(NULL)
{
}

DateUtil::~DateUtil()
{
	deinitLocaleData();
}

void DateUtil::deinitLocaleData()
{
	if (dateShortFormat)
		udat_close(dateShortFormat);
	if (dateMediumFormat)
		udat_close(dateMediumFormat);
	if (dateFullFormat)
		udat_close(dateFullFormat);
	if (timeFormat12H)
		udat_close(timeFormat12H);
	if (timeFormat24H)
		udat_close(timeFormat24H);
	dateShortFormat = NULL;
	dateMediumFormat = NULL;
	dateFullFormat = NULL;
	timeFormat12H = NULL;
	timeFormat24H = NULL;
}

int DateUtil::getDiffDaysFromToday()
{
	time_t now = time(NULL);
	DP_LOGD("todayStandardTime[%ld]", m_todayStandardTime);
	if (m_todayStandardTime == 0)
		return 0;
	else
		return getDiffDays(now, m_todayStandardTime);
}

int DateUtil::getDiffDays(time_t nowTime,time_t refTime)
{
	int diffDays = 0;
	int nowYear = 0;
	int nowYday = 0;
	int refYday = 0;
	int refYear = 0;
	struct tm *nowDate = localtime(&nowTime);
	nowYday = nowDate->tm_yday;
	nowYear = nowDate->tm_year;
	struct tm *finishedDate = localtime(&refTime);
	refYday = finishedDate->tm_yday;
	refYear = finishedDate->tm_year;
	diffDays = nowYday - refYday;
	/*DP_LOGD("refDate[%d/%d/%d]refTime[%ld]yday[%d]",
		(finishedDate->tm_year + 1900), (finishedDate->tm_mon + 1),
		finishedDate->tm_mday, refTime, refYday);*/
	if ((nowYear-refYear)>0 && diffDays < 0) {
		int year = nowDate->tm_year;
		diffDays = diffDays + 365;
		/* leap year */
		if ((year%4 == 0 && year%100 != 0) || year%400 == 0)
			diffDays++;
	}
	DP_LOGD("diffDays[%d]",diffDays);
	return diffDays;
}

UDateFormat *DateUtil::getBestPattern(const char *patternStr,
	UDateTimePatternGenerator *generator, const char *locale)
{
	UDateFormat *format = NULL;
	UChar customSkeleton[MAX_SKELETON_BUFFER_LEN] = {0,};
	UChar bestPattern[MAX_PATTERN_BUFFER_LEN] = {0,};
	UErrorCode status = U_ZERO_ERROR;
	int32_t patternLen = 0;

	if (patternStr) {
		u_uastrncpy(customSkeleton, patternStr, strlen(patternStr));
		patternLen = udatpg_getBestPattern(generator, customSkeleton,
		u_strlen(customSkeleton), bestPattern, MAX_PATTERN_BUFFER_LEN,
			&status);
		DP_LOGD("udatpg_getBestPattern status[%d] bestPattern[%s]", status,
			bestPattern);
		if (patternLen < 1) {
			format = udat_open(UDAT_SHORT, UDAT_NONE, locale, NULL, -1,
				NULL, -1, &status);
			return format;
		}
	}
	format = udat_open(UDAT_IGNORE, UDAT_NONE, locale, NULL, -1,
		bestPattern, -1, &status);
	return format;
}

void DateUtil::updateLocale()
{
	UDateTimePatternGenerator *generator = NULL;
	UErrorCode status = U_ZERO_ERROR;
	const char *locale = NULL;

	DP_LOGD_FUNC();

	deinitLocaleData();

	uloc_setDefault(getenv("LC_TIME"), &status);
	DP_LOGD("uloc_setDefault status[%d]",status);

	locale = uloc_getDefault();
	generator = udatpg_open(locale,&status);
	DP_LOGD("udatpg_open status[%d]",status);

	timeFormat12H = getBestPattern("hm", generator, locale);
	timeFormat24H = getBestPattern("Hm", generator, locale);

	dateShortFormat = udat_open(UDAT_NONE, UDAT_SHORT, locale, NULL, -1, NULL,
		-1, &status);
	dateMediumFormat = udat_open(UDAT_NONE, UDAT_MEDIUM, locale, NULL, -1, NULL,
		-1, &status);
	dateFullFormat = getBestPattern("yMMMEEEd", generator, locale);
	udatpg_close(generator);
}

void DateUtil::getDateStr(int style, double time, string &outBuf)
{
	UDateFormat *format = NULL;
	UErrorCode status = U_ZERO_ERROR;
	UChar str[MAX_BUF_LEN] = {0,};
	bool value = false;

	DP_LOGD_FUNC();
	switch (style) {
	case LOCALE_STYLE::TIME:
		if (runtime_info_get_value_bool(
				RUNTIME_INFO_KEY_24HOUR_CLOCK_FORMAT_ENABLED,&value) != 0) {
			DP_LOGE("runtime_info_get_value_bool is failed");
			format = timeFormat12H;
		} else {
			if (value)
				format = timeFormat24H;
			else
				format = timeFormat12H;
		}
		break;
	case LOCALE_STYLE::SHORT_DATE:
		format = dateShortFormat;
		break;
	case LOCALE_STYLE::MEDIUM_DATE:
		format = dateMediumFormat;
		break;
	case LOCALE_STYLE::FULL_DATE:
		format = dateFullFormat;
		break;
	default :
		DP_LOGE("Critical: cannot enter here");
		format = timeFormat12H;
	}
	if (format) {
		char tempBuf[MAX_BUF_LEN] = {0,};
		udat_format(format, time, str, MAX_BUF_LEN - 1, NULL, &status);
		DP_LOGD("udat_format : status[%d]", status);
		u_austrncpy(tempBuf, str, MAX_BUF_LEN-1);
		outBuf = string(tempBuf);
	} else {
		DP_LOGE("Critical: fail to get time value");
		outBuf = string(S_("IDS_COM_POP_ERROR"));
	}
}

