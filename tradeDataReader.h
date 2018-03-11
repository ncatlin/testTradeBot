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
#include <Windows.h>
#include "base_thread.h"
#include <vector>

class tradeDataReader : public base_thread
{
public:
	tradeDataReader(SafeQueue<UI_MESSAGE> *uiq, SafeQueue<UI_MESSAGE> *datafeedq)
		:base_thread() {
		uiMsgQueue = uiq;
		feedDataQueue = datafeedq;
	}
	~tradeDataReader() {};
	bool startFeedPipe();
	void stopFeedPipe();

private:
	void main_loop();
	void processCoinigyFeedMessage(int msglen);
	time_t tradeTimeToTimet(string timestring);

private:
	OVERLAPPED ov2 = { 0 };
	HANDLE hPipe;

	bool pipeStarted = false;
	bool connected = false;

	std::vector<char>buf;

	SafeQueue<UI_MESSAGE> *uiMsgQueue;
	SafeQueue<UI_MESSAGE> *feedDataQueue; //read by data engine, written by feed piper reader
};

