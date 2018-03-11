/*
Copyright 2018 Nia Catlin

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at
http://www.apache.org/licenses/LICENSE-2.0
Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/
#pragma once

#include "uiMessage.h"
#include "base_thread.h"
#include <vector>

class dataReplayThread : public base_thread
{
public:
	dataReplayThread(SafeQueue<UI_MESSAGE> *uiq, SafeQueue<UI_MESSAGE> *datafeedq, 
		QString pairCode, QString exchangeCd, std::vector<SHORT_PAIR_TRADE_RECORD> *recsPtr)
		:base_thread() {
		uiMsgQueue = uiq;
		feedDataQueue = datafeedq;
		paircode = pairCode;
		exchange = exchangeCd;
		records = recsPtr;
	}
	~dataReplayThread() {};

private:
	void main_loop();

private:
	QString paircode, exchange;
	std::vector<SHORT_PAIR_TRADE_RECORD> *records;

	bool pipeStarted = false;
	bool connected = false;

	std::vector<char>buf;

	SafeQueue<UI_MESSAGE> *uiMsgQueue;
	SafeQueue<UI_MESSAGE> *feedDataQueue; //read by data engine, written by feed piper reader
};

