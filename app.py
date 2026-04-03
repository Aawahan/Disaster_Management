from flask import Flask, render_template, request, jsonify, Response

import serial

import time

import cv2

from ultralytics import YOLO



app = Flask(__name__)



# -------- SERIAL SETUP --------

try:

    arduino = serial.Serial('/dev/ttyUSB0', 9600, timeout=1)

    time.sleep(2)

    print("Arduino connected")

except:

    arduino = None

    print("Arduino NOT connected")



# -------- CAMERA SETUP --------

cap = cv2.VideoCapture(0)

time.sleep(2)



if not cap.isOpened():

    print("âŒ Camera NOT opening")

else:

    print("âœ… Camera OK")



# Low resolution (important for Pi)

cap.set(3, 320)

cap.set(4, 240)



print("Camera opened:", cap.isOpened())



# -------- YOLO MODEL --------

model = YOLO("yolov8n.pt")



# -------- GLOBAL DATA --------

data = {

    "temp": "--",

    "hum": "--",

    "gas": "--",

    "left": "--",

    "right": "--",

    "distance": "--",

    "flame": "--",

    "roll": "--",

    "pitch": "--",

    "yaw": "--",

    "pump_mode": "AUTO",

    "pump_state": "OFF",

    "water": "OK",

    "battery": "100%",

    "people": 0

}



# -------- READ FROM ARDUINO --------

def read_serial():

    if not arduino:

        return



    try:

        if arduino.in_waiting > 0:

            line = arduino.readline().decode(errors='ignore').strip()



            parts = line.split(',')

            parsed = {}



            for part in parts:

                if ':' in part:

                    key, value = part.split(':')

                    parsed[key.strip()] = value.strip()



            data["temp"] = parsed.get("T", data["temp"])

            data["hum"] = parsed.get("H", data["hum"])

            data["gas"] = parsed.get("PPM", data["gas"])

            data["left"] = parsed.get("L", data["left"])

            data["right"] = parsed.get("R", data["right"])

            data["flame"] = parsed.get("F", data["flame"])



            data["roll"] = parsed.get("ROLL", data["roll"])

            data["pitch"] = parsed.get("PITCH", data["pitch"])

            data["yaw"] = parsed.get("YAW", data["yaw"])



            try:

                left = int(data["left"])

                right = int(data["right"])

                data["distance"] = (left + right) // 2

            except:

                data["distance"] = "--"



    except Exception as e:

        print("Serial error:", e)





# -------- VIDEO STREAM (STABLE VERSION) --------

def generate_frames():

    while True:

        ret, frame = cap.read()



        if not ret:

            print("Frame failed")

            continue



        # convert to jpeg

        ret, jpeg = cv2.imencode('.jpg', frame)



        if not ret:

            continue



        frame = jpeg.tobytes()



        yield (b'--frame\r\n'

               b'Content-Type: image/jpeg\r\n\r\n' + frame + b'\r\n')



# -------- ROUTES --------



@app.route('/')

def index():

    return render_template("index.html")



@app.route('/sensors')

def sensors():

    read_serial()

    return jsonify(data)



@app.route('/video_feed')

def video_feed():

    return Response(generate_frames(),

                    mimetype='multipart/x-mixed-replace; boundary=frame')



# -------- MOTOR --------

@app.route('/move')

def move():

    direction = request.args.get('dir')



    cmd_map = {

        "forward": "F",

        "backward": "B",

        "left": "L",

        "right": "R",

        "stop": "S"

    }



    if direction in cmd_map and arduino:

        arduino.write(cmd_map[direction].encode())



    return "OK"



# -------- PUMP --------

@app.route('/pump')

def pump():

    state = request.args.get('state')



    if arduino:

        if state == "on":

            arduino.write(b'P1')

            data["pump_mode"] = "MANUAL"

            data["pump_state"] = "ON"



        elif state == "off":

            arduino.write(b'P0')

            data["pump_mode"] = "MANUAL"

            data["pump_state"] = "OFF"



        elif state == "auto":

            arduino.write(b'A')

            data["pump_mode"] = "AUTO"



    return "OK"



# -------- TEXT --------

@app.route('/send_text')

def send_text():

    msg = request.args.get('msg')



    if msg and arduino:

        arduino.write((msg + "\n").encode())



    return "OK"



# -------- AUDIO --------

@app.route('/audio')

def audio():

    sound_type = request.args.get('type')



    if arduino:

        arduino.write((f"SOUND:{sound_type}\n").encode())



    return "OK"



# -------- MAIN --------

if __name__ == '__main__':

    app.run(host='0.0.0.0', port=5000, debug=True)

