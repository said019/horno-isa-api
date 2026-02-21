const express = require('express');
const cors = require('cors');
const app = express();

app.use(cors());
app.use(express.json());

// Estado global en memoria
let hornoState = {
  currentTemp: 0.0,
  targetTemp: 35.0,
  power: 0,
  status: "APAGADO"
};

// ESP32 hace POST aquí cada segundo
app.post('/api/sync', (req, res) => {
  const { currentTemp, power, status } = req.body;
  if (currentTemp !== undefined) hornoState.currentTemp = currentTemp;
  if (power !== undefined) hornoState.power = power;
  if (status !== undefined) hornoState.status = status;

  // Le respondemos al ESP32 con la meta actual
  res.json({ targetTemp: hornoState.targetTemp });
});

// La página web hace GET aquí para actualizar los números
app.get('/api/data', (req, res) => {
  res.json(hornoState);
});

// La página web hace POST aquí cuando mueves el slider
app.post('/api/setTarget', (req, res) => {
  const { targetTemp } = req.body;
  if (targetTemp !== undefined) hornoState.targetTemp = parseFloat(targetTemp);
  res.json({ success: true });
});

// Apagado de emergencia
app.post('/api/stop', (req, res) => {
  hornoState.targetTemp = 0;
  res.json({ success: true });
});

module.exports = app;
