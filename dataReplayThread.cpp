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
#include "dataReplayThread.h"
#include <iostream>
#include <Windows.h>

void dataReplayThread::main_loop()
{
	alive = true;

	size_t i = 0;
	for (auto it = records->begin(); it != records->end(); it++)
	{
		TRADEFEED_UPDATE *trademsg = new TRADEFEED_UPDATE;
		trademsg->rec.exchange = exchange;
		trademsg->rec.paircode = paircode;
		trademsg->rec.tradeType = it->tradeType;
		trademsg->rec.quantity = it->quantity;
		trademsg->rec.price = it->price;
		trademsg->rec.timecode = it->timecode;
		feedDataQueue->addItem(trademsg);
		i++;

	}

	TRADEFEED_UPDATE *trademsg = new TRADEFEED_UPDATE;
	trademsg->rec.tradeType = pairTradeType::eINVALID;
	trademsg->rec.exchange = exchange;
	trademsg->rec.paircode = paircode;
	feedDataQueue->addItem(trademsg);
	std::cout << "fed " << i << " records into trade engine" << endl;
	
	alive = false;
}