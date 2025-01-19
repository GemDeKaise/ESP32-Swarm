from flask import Flask, request, jsonify
from datetime import datetime, timedelta
from flask_cors import CORS
import time
import smtplib
from email.mime.text import MIMEText
from collections import defaultdict
from threading import Thread, Lock

app = Flask(__name__)
CORS(app)

sensor_data_storage = {}
sensor_history = defaultdict(list)
last_update_times = {}
alerts = [] 

SMTP_SERVER = "smtp.gmail.com"
SMTP_PORT = 465
EMAIL_ADDRESS = "x@gmail.com"
EMAIL_PASSWORD = "password"
RECIPIENT = "y@gmail.com"

ALERT_THRESHOLD = 0 
alerts_enabled = False
last_email_sent_time = None
email_lock = Lock()

INACTIVITY_TIMEOUT = 20

@app.route('/sensor-data', methods=['POST'])
def receive_data():
    try:
        data = request.get_json()
        if not data:
            return jsonify({"error": "Invalid or missing JSON"}), 400

        required_fields = ['chipID', 'temperature', 'humidity']
        if not all(field in data for field in required_fields):
            return jsonify({"error": "Missing required fields"}), 400

        chipID = data['chipID']
        temperature = data['temperature']
        humidity = data['humidity']
        timestamp = time.strftime('%Y-%m-%d %H:%M:%S', time.localtime())

        sensor_data_storage[chipID] = {
            'temperature': temperature,
            'humidity': humidity,
            'lastLogTime': timestamp
        }

        sensor_history[chipID].append({
            'time': timestamp,
            'temperature': temperature,
            'humidity': humidity
        })
        
        last_update_times[chipID] = time.time()

        return jsonify({"message": "Data received successfully"}), 200
    except Exception as e:
        print(f"Error: {e}")
        return jsonify({"error": "An error occurred"}), 500

@app.route('/average-temperature', methods=['GET'])
def calculate_average_temperature():
    try:
        now = datetime.now()
        two_minutes_ago = now - timedelta(minutes=2)
        temperatures = []

        for chipID, data in sensor_history.items():
            for record in data:
                record_time = datetime.strptime(record['time'], '%Y-%m-%d %H:%M:%S')
                if record_time >= two_minutes_ago:
                    temperatures.append(record['temperature'])

        if len(temperatures) == 0:
            return jsonify({"message": "No data in the last 2 minutes"}), 404

        average_temp = sum(temperatures) / len(temperatures)
        return jsonify({"average_temperature": average_temp}), 200
    except Exception as e:
        print(f"Error calculating average temperature: {e}")
        return jsonify({"error": "An error occurred"}), 500

@app.route('/sensor-history', methods=['GET'])
def get_sensor_history():
    return jsonify(sensor_history), 200

@app.route('/sensor-data', methods=['GET'])
def get_data():
    current_time = time.time()
    inactive_sensors = [chipID for chipID, last_time in last_update_times.items() if current_time - last_time > INACTIVITY_TIMEOUT]
    for chipID in inactive_sensors:
        sensor_data_storage.pop(chipID, None)
        last_update_times.pop(chipID, None)

    return jsonify(sensor_data_storage), 200

@app.route('/set-alert', methods=['POST'])
def set_alert():
    global ALERT_THRESHOLD, alerts_enabled
    data = request.get_json()
    if not data or 'threshold' not in data or 'enabled' not in data:
        return jsonify({"error": "Invalid request format"}), 400

    try:
        ALERT_THRESHOLD = float(data['threshold'])
        alerts_enabled = bool(data['enabled'])
        return jsonify({"message": "Alert settings updated"}), 200
    except ValueError:
        return jsonify({"error": "Invalid threshold value"}), 400

def send_alert_email(average_temp, affected_sensors):
    try:
        message_body = f"Alert! The average temperature dropped below {ALERT_THRESHOLD}°C.\n\n"
        message_body += f"Average Temperature: {average_temp}°C\n\n"
        message_body += "Sensor Details:\n"
        for sensor in affected_sensors:
            message_body += f"Chip ID: {sensor['chipID']}, Average Temp: {sensor['avg_temp']}°C\n"

        message = MIMEText(message_body)
        message['From'] = EMAIL_ADDRESS
        message['To'] = RECIPIENT
        message['Subject'] = "Temperature Alert"

        with smtplib.SMTP_SSL(SMTP_SERVER, SMTP_PORT) as server:
            server.login(EMAIL_ADDRESS, EMAIL_PASSWORD)
            server.sendmail(EMAIL_ADDRESS, RECIPIENT, message.as_string())
        print("Alert email sent successfully.")
    except Exception as e:
        print(f"Error sending alert email: {e}")

def check_temperature():
    global last_email_sent_time
    while True:
        if not alerts_enabled:
            time.sleep(10) 
            continue
        
        print("Checking alert");

        now = datetime.now()
        fifteen_minutes_ago = now - timedelta(minutes=15)
        temperatures = []
        affected_sensors = []

        for chipID, data in sensor_history.items():
            relevant_records = [
                record for record in data
                if datetime.strptime(record['time'], '%Y-%m-%d %H:%M:%S') >= fifteen_minutes_ago
            ]
            if relevant_records:
                avg_temp = sum(r['temperature'] for r in relevant_records) / len(relevant_records)
                temperatures.append(avg_temp)
                affected_sensors.append({"chipID": chipID, "avg_temp": avg_temp})

        if temperatures:
            temperatures = [float(temp) for temp in temperatures]
            average_temp = sum(temperatures) / len(temperatures)
            print(f"Average temperature: {average_temp}°C")
            print(f"Alert threshold: {ALERT_THRESHOLD}°C")
            if average_temp < ALERT_THRESHOLD:
                with email_lock:
                    if last_email_sent_time is None or (now - last_email_sent_time).total_seconds() > 120:
                        send_alert_email(average_temp, affected_sensors)
                        last_email_sent_time = now

        time.sleep(10)

Thread(target=check_temperature, daemon=True).start()

def run_http():
    app.run(host='0.0.0.0', port=8001)

def run_https():
    app.run(host='0.0.0.0', port=8000, ssl_context=('cert.pem', 'cert.key'))

if __name__ == '__main__':
    Thread(target=run_http).start()
    Thread(target=run_https).start()
