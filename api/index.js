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
  status: "APAGADO",
  timerActive: false,
  remainingTime: 0 // en segundos
};

// ESP32 hace POST aquí cada segundo
app.post('/api/sync', (req, res) => {
  const { currentTemp, power, status } = req.body;
  if (currentTemp !== undefined) hornoState.currentTemp = currentTemp;
  if (power !== undefined) hornoState.power = power;
  if (status !== undefined) hornoState.status = status;

  // Lógica del temporizador
  if (hornoState.timerActive && hornoState.remainingTime > 0) {
    hornoState.remainingTime -= 1;
    if (hornoState.remainingTime <= 0) {
      hornoState.timerActive = false;
      hornoState.targetTemp = 0;
      hornoState.status = "COMPLETADO";
      hornoState.remainingTime = 0;
    }
  }

  // Le respondemos al ESP32 con la meta actual
  res.json({ targetTemp: hornoState.targetTemp });
});

// La página web hace GET aquí para actualizar los números
app.get('/api/data', (req, res) => {
  res.json(hornoState);
});

// Configurar temporizador (recibe horas)
app.post('/api/setTimer', (req, res) => {
  const { hours } = req.body;
  if (hours !== undefined && hours > 0) {
    hornoState.remainingTime = Math.floor(parseFloat(hours) * 3600);
    hornoState.timerActive = true;
    res.json({ success: true, remainingTime: hornoState.remainingTime });
  } else {
    hornoState.timerActive = false;
    hornoState.remainingTime = 0;
    res.json({ success: true, message: "Temporizador detenido" });
  }
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
  hornoState.timerActive = false;
  hornoState.remainingTime = 0;
  res.json({ success: true });
});

module.exports = app;
