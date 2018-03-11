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
#include "tradeDataReader.h"
#include <iostream>
#include <string>
#include <rapidjson\document.h>
#include <rapidjson\reader.h>
#include "pipenames.h"


using namespace std;

bool tradeDataReader::startFeedPipe()
{
	if (pipeStarted)
	{
		addUILogMsg("Error: Refusing to start already started feed pipe", uiMsgQueue);
		cerr << "Error: Refusing to start already started feed pipe" << endl;
		return false;
	}

	wstring feedPipe = wstring(FEEDPIPENAME);
	hPipe = CreateNamedPipe(feedPipe.c_str(),
		PIPE_ACCESS_INBOUND | FILE_FLAG_OVERLAPPED, PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE,
		255, 64, 56 * 1024, 300, NULL);

	ov2 = { 0 };
	ov2.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
	if (!ov2.hEvent)
	{
		wcerr << "ERROR - Failed to create overlapped event in module handler" << endl;
		return false;
	}

	if (ConnectNamedPipe(hPipe, &ov2))
	{
		wcerr << "[rgat]Failed to ConnectNamedPipe Error: " << GetLastError();
		alive = false;
		return false;
	}

	pipeStarted = true;
	return true;
}

void tradeDataReader::stopFeedPipe()
{
	if (!pipeStarted)
	{
		addUILogMsg("Error: Can't stop non-started feed pipe", uiMsgQueue);
		cerr << "Error: Can't stop non-started feed pipe" << endl;
		return;
	}

	pipeStarted = false;
	CloseHandle(hPipe);
}

void tradeDataReader::main_loop()
{
	if (!startFeedPipe()) {
		addUILogMsg("Error: Failed to create a feed pipe", uiMsgQueue);
		return;
	}

	addUILogMsg("Waiting for Bittrex market data feed connection", uiMsgQueue);

	buf.resize(56 * 1024, 0);

	while (true)
	{
		if (!pipeStarted)
		{
			Sleep(500);
			continue;
		}

		DWORD bread = 0;
		ReadFile(hPipe, &buf.at(0), 56 * 1024, &bread, &ov2);

		while (true)
		{
			int res = WaitForSingleObject(ov2.hEvent, 300);
			if (res != WAIT_TIMEOUT) break;

			int gle = GetLastError();
			if (gle == ERROR_BROKEN_PIPE)
			{
				if (!pipeStarted) break;
				connected = false;
				addUILogMsg("Feed server disconnected. Waiting for new connection", uiMsgQueue);
				UI_FEED_CONNECTION *conmsg = new UI_FEED_CONNECTION;
				conmsg->tradeFeed = true;
				conmsg->connected = false;
				uiMsgQueue->addItem(conmsg);
				stopFeedPipe();
				startFeedPipe();
				break;
			}
		
			if (gle != ERROR_PIPE_LISTENING && connected == false) {
				connected = true;
				addUILogMsg("Feed server conencted", uiMsgQueue);
				UI_FEED_CONNECTION *conmsg = new UI_FEED_CONNECTION;
				conmsg->tradeFeed = true;
				conmsg->connected = true;
				uiMsgQueue->addItem(conmsg);
			}


			if (die) {
				cout << "Feed read exit" << endl;
				alive = false;
				return;
			}
		}

		int res2 = GetOverlappedResult(hPipe, &ov2, &bread, false);
		buf[bread] = 0;

		if (bread)
		{
			processCoinigyFeedMessage(bread);
		}
		else
		{
			//not sure this ever gets called, read probably fails?
			int err = GetLastError();
			if (err != ERROR_BROKEN_PIPE && err != ERROR_PIPE_LISTENING && !die)
			{
				if (!pipeStarted) break;
				if (err != ERROR_IO_INCOMPLETE)
					cerr << "Error: readpipe ReadFile error: " << err << endl;
			}
		}
	}
}

time_t tradeDataReader::tradeTimeToTimet(string timestring)
{
	//timestring
	//"2018-01-02T21:17:46"
	///0123456789012345678 - offset

	tm timeinfo;
	int year = std::stoi(timestring.substr(0, 4));
	timeinfo.tm_year = year - 1900;

	int monthsSinceJan = std::stoi(timestring.substr(5, 7));
	timeinfo.tm_mon = monthsSinceJan - 1;

	int dayOfMonth = std::stoi(timestring.substr(8, 10));
	timeinfo.tm_mday = dayOfMonth;

	int hour = std::stoi(timestring.substr(11, 13));
	timeinfo.tm_hour = hour;

	int min = std::stoi(timestring.substr(14, 16));
	timeinfo.tm_min = min;

	int sec = std::stoi(timestring.substr(17, 19));
	timeinfo.tm_sec = sec;

	return mktime(&timeinfo);
}

void tradeDataReader::processCoinigyFeedMessage(int msglen)
{
	if (!pipeStarted)
		return;

	std::string str(buf.begin(), buf.begin()+msglen);

	rapidjson::Document document;
	document.Parse(&str.at(0));

	TRADEFEED_UPDATE *trademsg = new TRADEFEED_UPDATE;
	trademsg->rec.paircode = document["label"].GetString();
	trademsg->rec.paircode.replace("/", "-");
	string typestring = document["type"].GetString();

	if (typestring == "BUY")
		trademsg->rec.tradeType = pairTradeType::eBuy;
	else if (typestring == "SELL")
		trademsg->rec.tradeType = pairTradeType::eSell;
	else
		trademsg->rec.tradeType = pairTradeType::eINVALID;

	trademsg->rec.quantity = document["quantity"].GetDouble();
	trademsg->rec.price = document["price"].GetDouble();

	string timestring = document["time"].GetString();	
	trademsg->rec.timecode = tradeTimeToTimet(timestring);

	feedDataQueue->addItem(trademsg);

}