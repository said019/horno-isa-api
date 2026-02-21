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
  remainingTime: 0, // en segundos
  kp: 2.0, // Valores por defecto
  ki: 0.5,
  kd: 1.0
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

  // Le respondemos al ESP32 con la meta actual y parámetros PID
  res.json({
    targetTemp: hornoState.targetTemp,
    kp: hornoState.kp,
    ki: hornoState.ki,
    kd: hornoState.kd
  });
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

// Configurar parámetros PID
app.post('/api/setPID', (req, res) => {
  const { kp, ki, kd } = req.body;
  if (kp !== undefined) hornoState.kp = parseFloat(kp);
  if (ki !== undefined) hornoState.ki = parseFloat(ki);
  if (kd !== undefined) hornoState.kd = parseFloat(kd);
  res.json({ success: true, kp: hornoState.kp, ki: hornoState.ki, kd: hornoState.kd });
});

// La página web hace POST aquí cuando mueves el slider
app.post('/api/setTarget', (req, res) => {
  let { targetTemp } = req.body;
  targetTemp = parseFloat(targetTemp);

  if (isNaN(targetTemp)) {
    return res.status(400).json({ success: false, error: "Valor no válido" });
  }

  // LÍMITE 1: Máximo 100 grados
  if (targetTemp > 100) {
    return res.status(400).json({ success: false, error: "La temperatura máxima es 100°C" });
  }

  // LÍMITE 2: Máximo salto de 5 grados
  const delta = Math.abs(targetTemp - hornoState.targetTemp);
  if (delta > 5.1) { // 5.1 para dar un margen pequeño de redondeo
    return res.status(400).json({ success: false, error: "Por seguridad, el cambio máximo permitido es de 5°C por vez." });
  }

  hornoState.targetTemp = targetTemp;
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
