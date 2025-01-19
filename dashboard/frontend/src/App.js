import React, { useState, useEffect } from 'react';
import axios from 'axios';
import { CircularProgressbar, buildStyles } from 'react-circular-progressbar';
import 'react-circular-progressbar/dist/styles.css';
import { BrowserRouter as Router, Routes, Route, Link } from 'react-router-dom';
import Chart from 'react-apexcharts';

const AlertPopup = ({ message, type, onClose }) => {
    return (
        <div className={`fixed top-4 right-4 px-6 py-4 rounded-lg shadow-lg text-white ${type === 'success' ? 'bg-green-500' : 'bg-red-500'}`}>
            <div className="flex items-center justify-between">
                <p>{message}</p>
                <button onClick={onClose} className="ml-4 font-bold">X</button>
            </div>
        </div>
    );
};

const Home = ({ sensorData }) => {
    const [alertThreshold, setAlertThreshold] = useState(0);
    const [alertsEnabled, setAlertsEnabled] = useState(false);
    const [averageTemperature, setAverageTemperature] = useState(null);
    const [popup, setPopup] = useState({ show: false, message: '', type: '' });

    const handleSetAlert = async () => {
        try {
            await axios.post('http://34.91.131.136:8001/set-alert', {
                threshold: parseFloat(alertThreshold),
                enabled: alertsEnabled,
            });
            setPopup({ show: true, message: 'Alert settings updated successfully!', type: 'success' });
        } catch (error) {
            setPopup({ show: true, message: 'Failed to update alert settings.', type: 'error' });
        }
    };

    const fetchAverageTemperature = async () => {
        try {
            const response = await axios.get('http://34.91.131.136:8001/average-temperature');
            setAverageTemperature(response.data.average_temperature);
        } catch (error) {
            console.error('Error fetching average temperature:', error);
        }
    };

    useEffect(() => {
        fetchAverageTemperature();
        const interval = setInterval(fetchAverageTemperature, 2000);
        return () => clearInterval(interval);
    }, []);

    return (
        <div className="min-h-screen bg-gradient-to-r from-blue-500 to-purple-600 flex flex-col items-center p-8">
            {popup.show && (
                <AlertPopup
                    message={popup.message}
                    type={popup.type}
                    onClose={() => setPopup({ ...popup, show: false })}
                />
            )}

            <h1 className="text-5xl font-bold mb-8 text-white">Sensor Data Dashboard</h1>

            <div className="flex flex-wrap justify-center gap-8 mb-8">
                <div className="p-6 bg-white shadow-lg rounded-lg w-96">
                    <label className="block text-gray-700 font-bold mb-2">Alert Threshold (°C):</label>
                    <input
                        type="number"
                        value={alertThreshold}
                        onChange={(e) => setAlertThreshold(e.target.value)}
                        className="mb-4 px-4 py-2 border rounded shadow w-full"
                    />
                    <label className="block text-gray-700 font-bold mb-2 flex items-center">
                        Enable Alerts:
                        <input
                            type="checkbox"
                            checked={alertsEnabled}
                            onChange={(e) => setAlertsEnabled(e.target.checked)}
                            className="ml-2"
                        />
                    </label>
                    <button
                        onClick={handleSetAlert}
                        className="mt-4 px-4 py-2 bg-blue-500 text-white rounded-full shadow hover:bg-blue-600 transition w-full"
                    >
                        Set Alert
                    </button>
                </div>

                <div className="p-6 bg-white shadow-lg rounded-lg w-96 text-center">
                    <h2 className="text-2xl font-bold text-gray-700">Current Average Temperature</h2>
                    <p className="text-4xl font-bold text-blue-500 mt-4">
                        {averageTemperature !== null ? `${averageTemperature.toFixed(2)}°C` : 'Loading...'}
                    </p>
                </div>
            </div>

            <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-12">
                {Object.entries(sensorData).map(([chipID, data]) => (
                    <div
                        key={chipID}
                        className="bg-white shadow-lg rounded-lg p-14 border border-gray-300">
                        <h2 className="text-3xl font-bold mb-4 text-gray-800">Chip ID: {chipID}</h2>
                        <p className="text-gray-600 mb-8 bg-gray-100 p-2 rounded shadow">Last Log Time: {data.lastLogTime}</p>
                        <div className="flex flex-col items-center space-y-8">
                            <div className="w-36 h-36">
                                <CircularProgressbar
                                    value={data.temperature}
                                    maxValue={50}
                                    text={`${data.temperature}°C`}
                                    styles={buildStyles({
                                        textColor: "#f88",
                                        pathColor: "#f00",
                                        trailColor: "#ddd"
                                    })}
                                />
                            </div>
                            <div className="w-36 h-36">
                                <CircularProgressbar
                                    value={data.humidity}
                                    maxValue={100}
                                    text={`${data.humidity}%`}
                                    styles={buildStyles({
                                        textColor: "#3498db",
                                        pathColor: "#00f",
                                        trailColor: "#ddd"
                                    })}
                                />
                            </div>
                            <Link 
                                to={`/details/${chipID}`} 
                                className="mt-4 px-4 py-2 bg-blue-500 text-white rounded-full shadow hover:bg-blue-600 transition">
                                View Details
                            </Link>
                        </div>
                    </div>
                ))}
            </div>
        </div>
    );
};

const Details = ({ chipID, historyData }) => {
    const chartOptions = {
        chart: {
            id: 'sensor-data-chart'
        },
        xaxis: {
            categories: historyData.map(entry => new Date(entry.time).toLocaleTimeString())
        }
    };

    const chartSeries = [
        {
            name: 'Temperature (°C)',
            data: historyData.map(entry => entry.temperature)
        },
        {
            name: 'Humidity (%)',
            data: historyData.map(entry => entry.humidity)
        }
    ];

    return (
        <div className="min-h-screen bg-gradient-to-r from-blue-500 to-purple-600 flex flex-col items-center p-8">
            <Link
                to="/"
                className="mb-6 px-6 py-2 bg-white text-blue-500 rounded-full shadow-lg hover:bg-gray-200 transition">
                Home
            </Link>
            <h1 className="text-4xl font-bold mb-8 text-white">Details for Chip ID: {chipID}</h1>
            <div className="bg-white shadow-lg rounded-lg p-10 w-full max-w-5xl">
                <Chart options={chartOptions} series={chartSeries} type="line" height={450} />
            </div>
        </div>
    );
};

const App = () => {
    const [sensorData, setSensorData] = useState({});
    const [history, setHistory] = useState({});

    useEffect(() => {
        const fetchData = async () => {
            try {
                const response = await axios.get('http://34.91.131.136:8001/sensor-data');
                setSensorData(response.data);
            } catch (error) {
                console.error('Error fetching sensor data:', error);
            }
        };

        const fetchHistory = async () => {
            try {
                const response = await axios.get('http://34.91.131.136:8001/sensor-history');
                setHistory(response.data);
            } catch (error) {
                console.error('Error fetching sensor history:', error);
            }
        };

        fetchData();
        fetchHistory();
        const interval = setInterval(() => {
            fetchData();
            fetchHistory();
        }, 2000);
        return () => clearInterval(interval);
    }, []);

    return (
        <Router>
            <Routes>
                <Route path="/" element={<Home sensorData={sensorData} />} />
                <Route
                    path="/details/:chipID"
                    element={<Details chipID={Object.keys(sensorData)[0]} historyData={history[Object.keys(sensorData)[0]] || []} />}
                />
            </Routes>
        </Router>
    );
};

export default App;
