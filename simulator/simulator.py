#!/usr/local/bin/python3.6

import sys
import logging
import argparse
import asyncio
import json
import websockets
from websockets.exceptions import ConnectionClosed

logging.basicConfig(level=logging.INFO,
                    format='%(asctime)-15s %(levelname)-8s %(message)s')
logger = logging.getLogger("default")

connected = set()
wilco = None


class Wilco:
    def __init__(self, channels=2):
        self._channels = channels
        self._values = {}
        self._keys = []
        for i in range(channels):
            key = str(i)
            self._keys.append(key)
            self._values[key] = 0

    def get_status(self):
        return json.dumps(self._values, ensure_ascii=True)

    def process_message(self, message):
        try:
            data = json.loads(message)
            for key in self._keys:
                if key in data:
                    value = data[key]
                    if type(value) is int:
                        if 0 <= value < 1024:
                            self._values[key] = value
                        else:
                            logger.warning("Value out of bounds: Value: %d", value)
                    else:
                        logger.warning("Value is not int. Real type: %s", type(value))
        except json.JSONDecodeError as e:
            logger.error("Invalid json")
        except:
            logger.error("Unexpected error: %s", sys.exc_info()[0])
            raise


async def suppress(f, *, exception):
    try:
        return await f
    except exception:
        pass


async def broadcast(message):
    global connected
    broadcasts = (ws.send(message) for ws in connected)
    broadcasts = [suppress(f, exception=ConnectionClosed) for f in broadcasts]
    await asyncio.wait(broadcasts)


async def wilco_simulator(websocket, path):
    global connected
    global wilco
    connected.add(websocket)
    await websocket.send(wilco.get_status())
    while True:
        try:
            message = await websocket.recv()
            logger.info("Received message: %s from: %s",
                        message, websocket.remote_address)
            wilco.process_message(message)
        except ConnectionClosed:
            connected.remove(websocket)
            break
        else:
            logger.info("State changed - broadcasting new state")
            await broadcast(wilco.get_status())


def parse_arguments(argv):
    parser = argparse.ArgumentParser()
    parser.add_argument("-p", "--port", type=int,
                        help="Port of simulator", default=8765)
    parser.add_argument("-c", "--channels", type=int,
                        help="Number of channels", default=2)
    return parser.parse_args(argv)


def main(argv):
    args = parse_arguments(argv[1:])
    logger.info("Starting Wilco server on port: %d", args.port)
    logger.info("Number of available channels: %s", args.channels)

    global wilco
    wilco = Wilco(args.channels)

    event_loop = asyncio.get_event_loop()
    event_loop.run_until_complete(
        websockets.serve(wilco_simulator, 'localhost', args.port))
    asyncio.get_event_loop().run_forever()


if __name__ == '__main__':
    main(sys.argv)
