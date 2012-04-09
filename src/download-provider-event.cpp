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
 * @file	download-provider-event.cpp
 * @author	Jungki Kwak (jungki.kwak@samsung.com)
 * @brief	download event class for event flow
 */
#include "download-provider-event.h"
#include "download-provider-common.h"

#include <iostream>

void Subject::attach(Observer *o)
{
	_observers.push_back(o);
}

void Subject::detach(Observer *o)
{
	vector<Observer*>::iterator it;
	for(it = _observers.begin() ; it < _observers.end() ; it++) {
		if (*it == o) {
			_observers.erase(it);
			break;
		}
	}
}

void Subject::notify(void)
{
	vector<Observer*>::iterator it;
	Observer *curObserver;
	it = _observers.begin();
	while (it < _observers.end()) {
		curObserver = *it;

		DP_LOGD("[%s] Call Update",curObserver->name().c_str());
		(*it)->update(this);

		if (curObserver != *it)
			continue;

		it++;
	}
}

void Observer::update(Subject *s)
{
	call();
}

//Observer::Observer(updateFunction uf, void *data)
/* For debug */
Observer::Observer(updateFunction uf, void *data, const char *name)
	: m_updateFunction(uf)
	, m_userData(data)
{
	observerName = name;
}

void Observer::set(updateFunction uf, void *data)
{
	m_updateFunction = uf;
	m_userData = data;
}

void Observer::clear(void)
{
	m_updateFunction = 0;
	m_userData = 0;
}

void Observer::call(void)
{
	if (m_updateFunction)
		m_updateFunction(m_userData);
}
