# testTradeBot
A prototype cryptocurrency trade display/simulation/bot GUI

This is the source code for a blog post: http://tbinarii.blogspot.co.uk/2018/03/project-writeup-experimenting-with-high.html

![image](https://github.com/ncatlin/ncatlin/raw/master/currenciesPane.png)

![image](https://github.com/ncatlin/ncatlin/raw/master/tradeSettings.png)

![image](https://github.com/ncatlin/ncatlin/raw/master/exampleTradeResults.png)

Realtime trade data display requires running the coinigyWebsocketsFeed.py script, which in turn requires a json file with your Coinigy api key/secret with websockets permissions granted. If you can compile it you can probably figure it out.

Important files are:

tradeEngine - reads price data and looks for trade triggers, pushes them to the market history

marketHistory - An object created for each coin pair, stores price activity and handles active trades

dataReplayThread - pushes historical data to the trade engine when running backtests

There is also tradeInterface which pushes trade triggers to a python script that performs actual trades. Not published because it will make you poor.
