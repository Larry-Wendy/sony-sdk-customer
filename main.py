from flask import Flask
from flask_socketio import SocketIO
import matplotlib.pyplot as plt
import matplotlib.image as mpimg
import time

app = Flask(__name__)
app.config['SECRET_KEY'] = 'secret!'
socketio = SocketIO(app)


@app.route("/take_photo")
def take_photo():
    socketio.emit('server_message', "image")
    print("image")
    return 'Photo taken', 200


@socketio.on('client_message')
def handle_message(message):
    print("Received message:", message)
    socketio.emit('server_message', message)
    time.sleep(2)
    image = mpimg.imread(message)
    plt.imshow(image)  # 显示图片
    plt.axis('off')  # 不显示坐标轴
    plt.show()

    return message


if __name__ == '__main__':
    socketio.run(app, port=5000, allow_unsafe_werkzeug=True)
