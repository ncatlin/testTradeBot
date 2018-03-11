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
#include <uiMessage.h>

void addUILogMsg(QString msg, SafeQueue<UI_MESSAGE> *uiMsgQueue)
{
	//todo : timestamp
	UI_STRING_MESSAGE *initmsg = new UI_STRING_MESSAGE;
	initmsg->msgType = uiMsgType::eTextLog;
	initmsg->stringData = msg;
	uiMsgQueue->addItem(initmsg);
}