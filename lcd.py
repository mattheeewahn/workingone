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
        ret, buffer = cv2.imencode('.jpg', frame)
        if not ret:
            continue
        frame_bytes = buffer.tobytes()
        yield (
            b'--frame\r\n'
            b'Content-Type: image/jpeg\r\n\r\n' + frame_bytes + b'\r\n'
        )

def serial_reader():
    global ser
    while True:
        try:
            if ser and ser.is_open and ser.in_waiting:
                line = ser.readline().decode(errors='ignore').strip()
                if not line:
                    continue

                print(f"[SERIAL] {line}")

                with state_lock:
                    robot_state["last_serial"] = line

                    if line.startswith("STATUS:"):
                        # 예: STATUS:FORWARD:123.4:OK
                        parts = line.split(":")
                        if len(parts) >= 4:
                            robot_state["motion"] = parts[1]
                            try:
                                robot_state["distance_cm"] = float(parts[2])
                            except:
                                robot_state["distance_cm"] = None
                            robot_state["locked"] = (parts[3] == "LOCKED")
                            if not robot_state["locked"] and not line.startswith("ALERT:"):
                                if robot_state["warning"].startswith("장애물"):
                                    robot_state["warning"] = ""

                        elif len(parts) >= 2:
                            robot_state["motion"] = parts[1]

                    elif line.startswith("ALERT:OBSTACLE:"):
                        dist = line.split(":")[-1]
                        robot_state["warning"] = f"장애물 감지! 약 {dist} cm 앞에 물체가 있습니다."
                        robot_state["motion"] = "STOPPED"
                        robot_state["locked"] = True

                    elif line.startswith("ALERT:BLOCKED:"):
                        dist = line.split(":")[-1]
                        robot_state["warning"] = f"아직 장애물이 있습니다. 약 {dist} cm"
                        robot_state["motion"] = "STOPPED"
                        robot_state["locked"] = True

                    elif line.startswith("INFO:Obstacle cleared"):
                        robot_state["warning"] = ""
                        robot_state["locked"] = False

            time.sleep(0.05)
        except Exception as e:
            print(f"[!] Serial reader error: {e}")
            time.sleep(1)

HTML = """
<!DOCTYPE html>
<html lang="ko">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Robot Control + Camera</title>
    <style>
        body {
            margin: 0;
            padding: 20px;
            background: #101418;
            color: white;
            font-family: Arial, sans-serif;
            text-align: center;
        }
        .video-box {
            border: 3px solid #333;
            border-radius: 14px;
            overflow: hidden;
            background: black;
            display: inline-block;
        }
        .video-box img {
            width: 640px;
            max-width: 95vw;
            height: auto;
            display: block;
        }
        .controls {
            margin-top: 20px;
            display: grid;
            grid-template-columns: 100px 100px 100px;
            grid-template-rows: 60px 60px 60px;
            gap: 10px;
            justify-content: center;
        }
        button {
            border: none;
            border-radius: 12px;
            font-size: 18px;
            font-weight: bold;
            cursor: pointer;
            background: #1f6feb;
            color: white;
        }
        .stop {
            background: #d93f3f;
        }
        .status {
            margin-top: 12px;
            color: #8fd19e;
            font-size: 18px;
        }
        .warning {
            margin-top: 12px;
            color: #ffd166;
            font-size: 20px;
            font-weight: bold;
            min-height: 28px;
        }
        .distance {
            margin-top: 8px;
            color: #cccccc;
        }
        .locked {
            margin-top: 8px;
            color: #ff7b7b;
        }
    </style>
</head>
<body>
    <h1>배달로봇 조종 + 실시간 카메라</h1>

    <div class="video-box">
        <img src="/video_feed" alt="Live Camera Feed">
    </div>

    <div class="controls">
        <div></div>
        <button onmousedown="sendCommand('W')" onmouseup="sendCommand('Q')"
                ontouchstart="sendCommand('W')" ontouchend="sendCommand('Q')">▲</button>
        <div></div>

        <button onmousedown="sendCommand('A')" onmouseup="sendCommand('Q')"
                ontouchstart="sendCommand('A')" ontouchend="sendCommand('Q')">◀</button>
        <button class="stop" onclick="sendCommand('Q')">■</button>
        <button onmousedown="sendCommand('D')" onmouseup="sendCommand('Q')"
                ontouchstart="sendCommand('D')" ontouchend="sendCommand('Q')">▶</button>

        <div></div>
        <button onmousedown="sendCommand('S')" onmouseup="sendCommand('Q')"
                ontouchstart="sendCommand('S')" ontouchend="sendCommand('Q')">▼</button>
        <div></div>
    </div>

    <div class="status" id="status">상태: 대기중</div>
    <div class="distance" id="distance">거리: --</div>
    <div class="locked" id="locked"></div>
    <div class="warning" id="warning"></div>

    <script>
        async function sendCommand(cmd) {
            try {
                const res = await fetch('/action/' + cmd, { method: 'POST' });
                const data = await res.json();

                if (data.status === "success") {
                    document.getElementById('status').innerText = '상태: ' + data.command;
                } else {
                    document.getElementById('status').innerText = '에러: ' + data.message;
                }
            } catch (e) {
                document.getElementById('status').innerText = '명령 전송 실패';
            }
        }

        async function pollRobotStatus() {
            try {
                const res = await fetch('/robot_status');
                const data = await res.json();

                document.getElementById('status').innerText = '상태: ' + data.motion;
                document.getElementById('distance').innerText =
                    '거리: ' + (data.distance_cm !== null ? data.distance_cm + ' cm' : '--');

                document.getElementById('warning').innerText = data.warning || '';
                document.getElementById('locked').innerText =
                    data.locked ? '장애물 정지 상태: 버튼을 다시 눌러야 재출발' : '';
            } catch (e) {
                console.log('status poll failed');
            }
        }

        document.addEventListener('keydown', function(e) {
            if (e.repeat) return;

            if (e.key === 'ArrowUp') sendCommand('W');
            else if (e.key === 'ArrowLeft') sendCommand('A');
            else if (e.key === 'ArrowDown') sendCommand('S');
            else if (e.key === 'ArrowRight') sendCommand('D');
            else if (e.key === ' ' || e.key.toLowerCase() === 'q') sendCommand('Q');
        });

        document.addEventListener('keyup', function(e) {
            if (
                e.key === 'ArrowUp' ||
                e.key === 'ArrowLeft' ||
                e.key === 'ArrowDown' ||
                e.key === 'ArrowRight'
            ) {
                sendCommand('Q');
            }
        });

        setInterval(pollRobotStatus, 300);
        pollRobotStatus();
    </script>
</body>
</html>
"""

@app.route('/')
def index():
    return render_template_string(HTML)

@app.route('/video_feed')
def video_feed():
    return Response(
        generate_frames(),
        mimetype='multipart/x-mixed-replace; boundary=frame'
    )

@app.route('/robot_status')
def get_robot_status():
    with state_lock:
        return jsonify(robot_state)

@app.route('/action/<cmd>', methods=['POST'])
def handle_action(cmd):
    cmd = cmd.upper()

    if cmd in ['W', 'A', 'S', 'D', 'Q']:
        if ser and ser.is_open:
            try:
                ser.write(cmd.encode())
                print(f"[+] Sent to Arduino: {cmd}")
                return jsonify({"status": "success", "command": cmd})
            except Exception as e:
                print(f"[!] Serial Write Failed: {e}")
                return jsonify({"status": "error", "message": f"Serial write failed: {e}"}), 500
        else:
            print("[-] Serial not opened")
            return jsonify({"status": "error", "message": "Serial disconnected"}), 500
    else:
        return jsonify({"status": "error", "message": "Invalid command"}), 400

if __name__ == '__main__':
    print("[*] Starting Flask Web Server...")

    if ser and ser.is_open:
        t = threading.Thread(target=serial_reader, daemon=True)
        t.start()

    app.run(host='0.0.0.0', port=5000, debug=False, threaded=True)
