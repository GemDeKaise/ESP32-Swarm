from flask import Flask, request, jsonify

app = Flask(__name__)

data_storage = []

@app.route('/sensor-data', methods=['POST'])
def receive_data():
    try:
        data = request.get_json()
        if not data:
            return jsonify({"error": "Invalid or missing JSON"}), 400

        # Log received data
        print(f"Received data: {data}")

        # Store the data
        data_storage.append(data)

        return jsonify({"message": "Data received successfully"}), 200
    except Exception as e:
        print(f"Error: {e}")
        return jsonify({"error": "An error occurred"}), 500

@app.route('/sensor-data', methods=['GET'])
def get_data():
    return jsonify(data_storage), 200

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=8000)
