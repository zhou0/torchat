import argparse
import json
import asyncio
import websockets
import time
import signal
from tc_sessions import MessageSession, ClientSession, AbstractSession

socketDict = {} # map all websockets to related classes

def get_hostname ():
    raise NotImplementedError("TOR Hostname")

async def open_session (websocket):
    buf = await websocket.recv()
    # if json not valid,explicit exception and connection closed
    try:
        j = AbstractSession.is_openJSON_acceptable(buf)
    except ValueError:
        # json in websocket.recv is invalid
        return None
    if j['open'] == 'message':
        return MessageSession(j, websocket)
    elif j['open'] == 'client':
        return ClientSession(j, websocket)
    elif j['open'] == 'file':
        raise NotImplementedError()

async def get_session(websocket):
    # grab the session related to communication over websocket
    # if the communication was opened by another node
    # open a new session and store it in socketDict
    global socketDict
    if websocket in socketDict:
        return socketDict[websocket]
    else:
        # the websocket was not opened by the local daemon
        session = await open_session(websocket)
        if session is not None:
            socketDict[websocket] = session
        return session

async def send_error_and_close(websocket):
    # TODO: follow protocol for errors
    # maybe analise jsonBuf for type of error
    # or maybe just use some ErrorSession
    await websocket.send(json.dumps({'error':'generic'}))
    websocket.close()

async def communication_with_node(websocket, path):
    session = await get_session(websocket)
    if session is None:
        # got an invalid json, do not start a communication with the other node
        print ("DAI ERRORE INDIETRO")
        await send_error_and_close(websocket)
        return
    # now loop until errors or connection closed
    while session.isAcceptable() and not session.toClose():
        await session.waitForAnotherJMU() # TODO await?
    # check if the session has to be closed for an error
    if session.getErrorState():
        await session.sendError()
    session.shutdown()
    websocket.close()
    return session

def assign_args(argv):
    parser = argparse.ArgumentParser(description="TORchat Daemon")
    parser.add_argument('--port', '-p', nargs='?', const=int, dest='port')
    parser.add_argument('--host', '-H', nargs='?', const=str, dest='host')
    args = vars(parser.parse_args(argv)) # arguments are mapped to dict
    print (args)
    port = 8000 if args['port'] is None else args['port']
    host = 'localhost' if args['host'] is None else args['host']

    return host, port


def prepare_to_shutdown(loop, logic):
    logic.close()
    loop.stop()

def main(argv):

    host, port = assign_args(argv)

    logic = websockets.serve(communication_with_node, host, port)
    loop = asyncio.get_event_loop()
    loop.add_signal_handler(signal.SIGINT, prepare_to_shutdown, loop, logic)
    loop.run_until_complete(logic)
    loop.run_forever()

if __name__ == '__main__':
    import sys
    main(sys.argv[1:])
