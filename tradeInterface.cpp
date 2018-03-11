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
#include "tradeInterface.h"
#include <iostream>
#include <QtCore/qstring.h>
#include "pipenames.h"

bool tradeInterface::tradeAlert(QString tradepair, double price)
{
	if (!isConnected)
		return false;

	char mushbuf[40];
	memset(&mushbuf, 0, 40);
	mushbuf[0] = 'T';
	mushbuf[1] = 'R';
	mushbuf[2] = 'A';
	mushbuf[3] = 'D';
	mushbuf[4] = 'E';

	char *mushptr = mushbuf;
	mushptr += 5;


	string coinp = tradepair.toStdString();
	memcpy(mushptr, coinp.data(), tradepair.size());
	mushptr += tradepair.size();

	QString pricestr = "!" + QString::number(price, 'f', 8) + "!";

	string prices = pricestr.toStdString();
	memcpy(mushptr, prices.data(), pricestr.size());

	WriteFile(hPipe, &mushbuf, 40, NULL, NULL);

	return true;

}


void tradeInterface::sendPrice(QString coincode, double price)
{
	if (!this->isConnected)
		return;

	char mushbuf[40];
	memset(&mushbuf, 0, 40);
	mushbuf[0] = 'P';
	mushbuf[1] = 'R';
	mushbuf[2] = 'I';
	mushbuf[3] = 'C';
	mushbuf[4] = 'E';

	string coinp = coincode.toStdString();
	char *mushptr = mushbuf;
	mushptr += 5;
	memcpy(mushptr, coinp.data(), coincode.size());
	mushptr += coincode.size();

	QString pricestr = "!"+QString::number(price, 'f', 8) + "!";

	string prices = pricestr.toStdString();
	memcpy(mushptr, prices.data(), pricestr.size());

	WriteFile(hPipe, &mushbuf, 40, NULL, NULL);
}

void tradeInterface::stopFeedPipe()
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


bool tradeInterface::startFeedPipe()
{
	if (hPipe)
	{
		CloseHandle(hPipe);
		hPipe = NULL;
	}

	wstring feedPipe = wstring(TRADEPIPENAME);
	hPipe = CreateNamedPipe(feedPipe.c_str(),
		PIPE_ACCESS_OUTBOUND, PIPE_TYPE_MESSAGE |PIPE_WAIT,
		255, 64, 56 * 1024, 300, NULL);


	pipeStarted = true;
	return true;

}

void tradeInterface::connectFeedPipe()
{
	startFeedPipe();
	addUILogMsg("Waiting for  trade interface connection", uiMsgQueue);
	ConnectNamedPipe(hPipe, NULL);
	addUILogMsg("Trade interface connected", uiMsgQueue);
	UI_FEED_CONNECTION *conmsg = new UI_FEED_CONNECTION;
	conmsg->tradeInterface = true;
	conmsg->connected = true;
	uiMsgQueue->addItem(conmsg);
	isConnected = true;
}

void tradeInterface::sendToFeed(QString msg)
{
	commsMutex.lock();
	itemslist.push_back(msg);
	commsMutex.unlock();
}

void tradeInterface::main_loop()
{
	connectFeedPipe();

	time_t pingtimer, timenow;
	time(&pingtimer);
	pingtimer += 4;

	char mushbuf[40];
	memset(&mushbuf, 0, 40);
	mushbuf[0] = 'P';
	mushbuf[1] = 'I';
	mushbuf[2] = 'N';
	mushbuf[3] = 'G';
	mushbuf[4] = 'G';

	while (true)
	{

		if (!isConnected)
		{
			connectFeedPipe();
		}


		time(&timenow);
		if (timenow > pingtimer)
		{
			if (!WriteFile(hPipe, &mushbuf, 40, NULL, NULL))
			{
				isConnected = false;
				addUILogMsg("Trade interface disconnected", uiMsgQueue);
				UI_FEED_CONNECTION *conmsg = new UI_FEED_CONNECTION;
				conmsg->tradeInterface = true;
				conmsg->connected = false;
				uiMsgQueue->addItem(conmsg);
			}
			
			pingtimer = timenow + 1;
		}


		while (isConnected && !itemslist.empty())
		{
			commsMutex.lock();
			QString msg = itemslist.front();
			itemslist.pop_front();
			commsMutex.unlock();

			bool itworked = WriteFile(hPipe, msg.toStdString().c_str(), msg.size(), NULL, NULL);
			if (!itworked)
			{
				commsMutex.lock();
				itemslist.push_front(msg);
				commsMutex.unlock();
				isConnected = false;
				addUILogMsg("Trade interface disconnected", uiMsgQueue);
				UI_FEED_CONNECTION *conmsg = new UI_FEED_CONNECTION;
				conmsg->tradeInterface = true;
				conmsg->connected = false;
				uiMsgQueue->addItem(conmsg);
			}
		}

	}
}