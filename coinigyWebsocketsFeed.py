import time
from socketclusterclient import Socketcluster
import json

#subscribes to conigy websockets and pushes the data over a pipe to our waiting readers
#in the current configuration it subscribes to BTC pairs on the Bittrex exchange

#this api key should only have permission to read feeds
#{"apiKey": "yourAPIkey", "apiSecret": "yourAPIsecret"}
CREDS_FILE_PATH="C:\\Users\\nia\\PycharmProjects\\autotrader\\coinigyCreds.json"
writePipeName = "\\\\.\\pipe\\coinigyFeedPipe"


writepipe = open(writePipeName, 'wb')

def fill_chan_list(socket):

    def trademessage(key, data):  # Messages will be received here
        writepipe.write(json.dumps(data, sort_keys=True).encode('UTF-8'))
        writepipe.flush()

    def getChannels(eventname, error, data):
        allChans = [x['channel'] for x in data[0] if x['channel'][-5:] == "--BTC"] #subscribe to bitcoin pair feds only
        tradeChannels = [x for x in allChans if x[:5] == "TRADE"]

        for chan in tradeChannels:
            socket.subscribe("WS"+chan)
            socket.onchannel("WS"+chan, trademessage)  # This is used for watching messages over channel

    socket.emitack("channels","BTRX",getChannels)

def onSetAuthentication(socket, token):
    #logging.info("Token received " + token)
    socket.setAuthtoken(token)


def onAuthentication(socket, isauthenticated):
    global api_credentials

    def ack(eventname, error, data):
        fill_chan_list(socket)

    socket.emitack("auth", api_credentials, ack)


if __name__ == "__main__":
    global api_credentials
    fd = open(CREDS_FILE_PATH, 'r')  # {apiKey:"xyx", apiSecret: "123"}
    api_credentials = json.load(fd)
    fd.close()

    while True:
        socket = Socketcluster.socket("wss://sc-02.coinigy.com/socketcluster/")

        socket.setAuthenticationListener(onSetAuthentication, onAuthentication)
        socket.setreconnection(False)
        socket.connect()
        time.sleep(5)
        print("Connected")
