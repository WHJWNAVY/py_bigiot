#!/usr/bin/python3
import socket
import time
import json
import random

# must be modified===
DEVICEID = "19493"
INPUTID = "17482"
APIKEY = "77e908022"
# modify end=========

BIGIOT_HOST = "www.bigiot.net"
BIGIOT_PORT = 8181
HEARTBEAT_TIMEOUT = 40
REPORT_TIMEOUT = 5
RECV_TIMEOUT = 5

DATA_END_FLAG = b"\n"
DATA_ZERO_FLAG = b""


def checkin_bytes(id, key):
    chkin_msg = {"M": "checkin", "ID": id, "K": key}
    chkin_json = json.dumps(chkin_msg)
    chkin_bytes = bytes(chkin_json, encoding = "utf8") + DATA_END_FLAG
    return chkin_bytes


def keep_online(sck, tmout):
    if time.time() - tmout > HEARTBEAT_TIMEOUT:
        say_msg={"M": "status"}
        say_json=json.dumps(say_msg)
        say_bytes=bytes(say_json, encoding="utf8") + DATA_END_FLAG
        sck.sendall(say_bytes)
        print("check status")
        return time.time()
    else:
        return tmout


def bigiot_say(sck, id, content):
    say_msg={"M": "say", "ID": id, "C": content}
    say_json=json.dumps(say_msg)
    sayBytes=bytes(say_json, encoding="utf8") + DATA_END_FLAG
    sck.sendall(sayBytes)


def bigiot_update(sck, dev_id, dat_id, content):
    say_msg={"M": "update", "ID": dev_id, "V": {dat_id: content}}
    say_json=json.dumps(say_msg)
    sayBytes=bytes(say_json, encoding="utf8") + DATA_END_FLAG
    sck.sendall(sayBytes)


def led_on():
    print("==========LED ON==========")


def led_off():
    print("==========LED OFF==========")


def sck_init(host, port, tmout):
    # connect bigiot
    sck=socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sck.settimeout(tmout)
    while True:
        try:
            sck.connect((host, port))
            break
        except:
            print("waiting for connect bigiot.net...")
            time.sleep(2)
    return sck


def report_data(sck, dev_id, dat_id, tmout):
    if time.time() - tmout > REPORT_TIMEOUT:
        dat_int=random.randint(1, 100)
        say_msg=str(dat_int)
        bigiot_update(sck, dev_id, dat_id, say_msg)
        print("report status")
        return time.time()
    else:
        return tmout

# deal with message coming in


def process(sck, msg):
    msg=json.loads(msg)
    msg_content=msg["M"]
    if msg_content == "connected":
        sck.sendall(checkin_bytes(DEVICEID, APIKEY))
    if msg_content == "login":
        msg_id=msg["ID"]
        bigiot_say(
            sck, msg_id, "Welcome! Your public ID is [{0}]".format(msg_id))
    if msg_content == "say":
        msg_id=msg["ID"]
        msg_cmd=msg["C"]
        bigiot_say(sck, msg_id, "You have send to me:{{0}}".format(msg_cmd))
        if msg_cmd == "play":
            # led.on()
            led_on()
            bigiot_say(sck, msg_id, "LED turns on!")
        elif msg_cmd == "stop":
            # led.off()
            led_off()
            bigiot_say(sck, msg_id, "LED turns off!")
        else:
            bigiot_say(sck, msg_id, "Unsupport command!")
            print("Unsupport command!")


if __name__ == "__main__":
    # connect bigiot
    SOCKET=sck_init(BIGIOT_HOST, BIGIOT_PORT, RECV_TIMEOUT)
    SOCKET.sendall(checkin_bytes(DEVICEID, APIKEY))

    # keep online with bigiot function
    RECV_DATA_BUFF=b""
    RECV_DATA_FLAG=True
    RECV_DATA_TIMEOUT=time.time()
    REPORT_DATA_TIMEOUT=time.time()

    # main while
    while True:
        try:
            rcvd=SOCKET.recv(1)
            RECV_DATA_FLAG=True
        except:
            RECV_DATA_FLAG=False
            time.sleep(1)
            RECV_DATA_TIMEOUT=keep_online(SOCKET, RECV_DATA_TIMEOUT)
            REPORT_DATA_TIMEOUT=report_data(
                SOCKET, DEVICEID, INPUTID, REPORT_DATA_TIMEOUT)
        if RECV_DATA_FLAG:
            if rcvd != DATA_END_FLAG:
                RECV_DATA_BUFF += rcvd
            else:
                msg=str(RECV_DATA_BUFF, encoding="utf-8")
                process(SOCKET, msg)
                print("Recv msg[{0}] from server".format(msg))
                RECV_DATA_BUFF=DATA_ZERO_FLAG
