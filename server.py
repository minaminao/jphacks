import base64
import json
import subprocess

import cv2
import eventlet
import numpy as np
import socketio
from estimate import pakupaku

sio = socketio.Server()
app = socketio.WSGIApp(sio, static_files={
    '/': {'content_type': 'text/html', 'filename': 'index.html'}
})

buffer = {}
np.set_printoptions(threshold=10000000)

# 予測モデルで予測を行うメソッド
def requestPrediction(textList):
    # response = subprocess.check_output(["python3", "estimate.py", ""])
    response = pakupaku(textList)
    return response

    # for index, image in enumerate(bufferImage):
    #     retval = cv2.imwrite(f"images/{str(index)}.jpg", image)
    #     print(retval)
    #     if index > 5:
    #         break

@sio.event
def connect(sid, environ):
    print('connect ', sid)
    buffer[sid] = []

@sio.event
def disconnect(sid):
    print('disconnect ', sid)

@sio.event
def message(sid, data):
    print('message ', data)

@sio.event
def sendImage(sid, data):
    try:
        # base64をdecode
        data = base64.b64decode(data)

        data = np.frombuffer(data, dtype=np.uint8)
        data = cv2.imdecode(data, cv2.IMREAD_COLOR)
        
        print(data.shape)

        # リサイズする
        data = cv2.resize(data,(160, 80))
        response = requestPrediction(data)
        response = ["good"]
        
    except:
        import traceback
        traceback.print_exc()
        response = []

    # 返ってきた値を返す
    sio.emit('requestPrediction', json.dumps({'data': response}), room=sid)

@sio.event
def sendText(sid, data):
    data = json.loads(data)

    # dataは、strを要素に持つリスト
    response = requestPrediction(data['text'])

    # 返ってきた値を返す
    sio.emit('requestPrediction', json.dumps({'data': response}), room=sid)

if __name__ == '__main__':
    eventlet.wsgi.server(eventlet.listen(('', 80)), app)