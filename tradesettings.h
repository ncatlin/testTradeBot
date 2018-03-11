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

#define TRADE_FEE (0.25)
#define MINIMUM_PROFIT_PERCENT ((TRADE_FEE*2)+0.05)
#define MINIMUM_SECONDS_UNTIL_PEAK (4)
//#define PUMP_TRIGGER_PERCENT (2.0)
#define PUMP_WATCH_DURATION (10*60)
#define PUMP_END_PERCENT (-7.9)
//#define MINIMUM_PUMP_10M_TRANSACTION_AVERAGE 0.8#
#define TRADE_START_DELAY 5

#define TIME_LIMIT_TO_BUY_ANY (40)
#define TIME_LIMIT_TO_BUY_ALL (80)


enum eTradeResult { eTargetWin, eStopLoss, ePartialWin, ePartialLoss, eFailBuyAny};

struct alertSettingsStruct {
	bool volDeltaRequired;
	float alertVolumeDelta;
	int alertVolumePeriod;

	bool priceIncreaseRequired;
	float alertPriceIncreaseDelta;
	int alertPriceIncreasePeriod;

	bool priceDecreaseRequired;
	float alertPriceDecreaseDelta;
	int alertPriceDecreasePeriod;

	bool minVolRequired;
	float minimumBtcVolumeCoin;
	int minimumBtcVolPeriod;

	bool minBuyPcRequired;
	float minBuyPcVal;
	int minBuyPcPeriod;
	
	bool maxBuyPCAllowed;
	float maxBuyPCVal;
	int maxBuyPCPeriod;
};

struct tradeSettingsStruct {

	float targetSellHigh;
	float stopLossLow;
	float lateExitPercent;
	float buyValue;
	time_t lateExitStart;
	int haltBuyAnyTime;
	int haltBuyAllTime;
	int hardTimeLimit;
};