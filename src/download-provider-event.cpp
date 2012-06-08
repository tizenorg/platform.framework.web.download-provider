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
