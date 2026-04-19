from flask import Flask, render_template_string, jsonify, Response
import serial
import time
import cv2
import threading
from picamera2 import Picamera2

app = Flask(__name__)

PORT = '/dev/ttyACM0'
BAUDRATE = 9600

robot_state = {
    "motion": "대기중",
    "distance_cm": None,
    "warning": "",
    "locked": False,
    "last_serial": ""
}

state_lock = threading.Lock()

try:
    ser = serial.Serial(PORT, BAUDRATE, timeout=1)
    time.sleep(2)
    print(f"[*] Serial Connected on {PORT}")
except Exception as e:
    print(f"[!] Serial Connection Failed: {e}")
    ser = None

picam2 = Picamera2()
picam2.configure(
    picam2.create_video_configuration(
        main={"size": (640, 480)}
    )
)
picam2.start()
time.sleep(1)

def generate_frames():
    while True:
        frame = picam2.capture_array()
        frame = cv2.cvtColor(frame, cv2.COLOR_RGB2BGR)
        frame = cv2.flip(frame, 0)
        ret, buffer = cv2.imencode('.jpg', frame)
        if not ret:
            continue
        yield (
            b'--frame\r\n'
            b'Content-Type: image/jpeg\r\n\r\n' + buffer.tobytes() + b'\r\n'
        )

def serial_reader():
    global ser
    while True:
        try:
            if ser and ser.is_open and ser.in_waiting:
                line = ser.readline().decode(errors='ignore').strip()
                if not line:
                    continue

                with state_lock:
                    robot_state["last_serial"] = line

                    if line.startswith("STATUS:"):
                        parts = line.split(":")
                        if len(parts) >= 4:
                            robot_state["motion"] = parts[1]
                            try:
                                robot_state["distance_cm"] = float(parts[2])
                            except:
                                robot_state["distance_cm"] = None
                            robot_state["locked"] = (parts[3] == "LOCKED")
                        elif len(parts) >= 2:
                            robot_state["motion"] = parts[1]

                    elif line.startswith("ALERT:OBSTACLE:"):
                        dist = line.split(":")[-1]
                        robot_state["warning"] = f"장애물 감지! 약 {dist} cm"
                        robot_state["motion"] = "STOPPED"
                        robot_state["locked"] = True

                    elif line.startswith("ALERT:BLOCKED:"):
                        dist = line.split(":")[-1]
                        robot_state["warning"] = f"아직 장애물 있음: {dist} cm"
                        robot_state["motion"] = "STOPPED"
                        robot_state["locked"] = True

                    elif line.startswith("INFO:Obstacle cleared"):
                        robot_state["warning"] = ""
                        robot_state["locked"] = False

            time.sleep(0.05)
        except:
            time.sleep(1)

HTML = """
<!DOCTYPE html>
<html lang="ko">
<head>
<meta charset="UTF-8">
<title>Robot Control</title>
<style>
body {background:#101418;color:white;text-align:center;font-family:Arial;}
.video-box {border:3px solid #333;border-radius:14px;overflow:hidden;display:inline-block;}
.video-box img {width:640px;max-width:95vw;}
.controls {
    margin-top:20px;
    display:grid;
    grid-template-columns:100px 100px 100px;
    grid-template-rows:60px 60px 60px;
    gap:10px;
    justify-content:center;
}
button {
    border:none;border-radius:12px;font-size:18px;font-weight:bold;
    cursor:pointer;background:#1f6feb;color:white;
}
.stop {background:#d93f3f;}
.status {margin-top:10px;color:#8fd19e;}
.warning {margin-top:10px;color:#ffd166;font-weight:bold;}
</style>
</head>
<body>

<h1>Control</h1>

<div class="video-box">
<img src="/video_feed">
</div>

<div class="controls">
    <div></div>

    <!-- ↑ (전진) -->
    <button onmousedown="sendCommand('D')" onmouseup="sendCommand('Q')"
            ontouchstart="sendCommand('D')" ontouchend="sendCommand('Q')">▲</button>

    <div></div>

    <!-- ← ■ → -->
    <button onmousedown="sendCommand('S')" onmouseup="sendCommand('Q')"
            ontouchstart="sendCommand('S')" ontouchend="sendCommand('Q')">◀</button>

    <button class="stop" onclick="sendCommand('Q')">■</button>

    <button onmousedown="sendCommand('W')" onmouseup="sendCommand('Q')"
            ontouchstart="sendCommand('W')" ontouchend="sendCommand('Q')">▶</button>

    <div></div>

    <!-- ↓ (후진) -->
    <button onmousedown="sendCommand('A')" onmouseup="sendCommand('Q')"
            ontouchstart="sendCommand('A')" ontouchend="sendCommand('Q')">▼</button>

    <div></div>
</div>

<div class="status" id="status">상태: 대기중</div>
<div id="warning"></div>

<script>
async function sendCommand(cmd){
    await fetch('/action/' + cmd, {method:'POST'});
}

async function poll(){
    const res = await fetch('/robot_status');
    const d = await res.json();
    document.getElementById('status').innerText = '상태: ' + d.motion;
    document.getElementById('warning').innerText = d.warning;
}

document.addEventListener('keydown', e=>{
    if(e.repeat) return;
    if(e.key==='ArrowUp') sendCommand('D');
    else if(e.key==='ArrowLeft') sendCommand('S');
    else if(e.key==='ArrowDown') sendCommand('A');
    else if(e.key==='ArrowRight') sendCommand('W');
    else if(e.key===' '||e.key==='q') sendCommand('Q');
});

document.addEventListener('keyup', e=>{
    if(['ArrowUp','ArrowLeft','ArrowDown','ArrowRight'].includes(e.key)){
        sendCommand('Q');
    }
});

setInterval(poll,300);
</script>

</body>
</html>
"""

@app.route('/')
def index():
    return render_template_string(HTML)

@app.route('/video_feed')
def video_feed():
    return Response(generate_frames(), mimetype='multipart/x-mixed-replace; boundary=frame')

@app.route('/robot_status')
def status():
    with state_lock:
        return jsonify(robot_state)

@app.route('/action/<cmd>', methods=['POST'])
def action(cmd):
    if ser and ser.is_open:
        ser.write(cmd.encode())
        return jsonify({"ok":True})
    return jsonify({"ok":False})

if __name__ == '__main__':
    if ser and ser.is_open:
        threading.Thread(target=serial_reader, daemon=True).start()
    app.run(host='0.0.0.0', port=5000, threaded=True)
