# 🔥 Horno ISA — Control Térmico IoT

Sistema de control de horno con ESP32 + Vercel. Arquitectura cliente-servidor con control PI, dimmer TRIAC y dashboard web en tiempo real.

## Arquitectura

```
ESP32 (Firmware)          Vercel (Nube)            Navegador Web
┌──────────────┐    HTTPS POST /api/sync     ┌──────────────┐
│ Núcleo 1:    │ ──────────────────────────→  │  Express API  │
│  Control PI  │ ←────────────────────────── │  (Serverless) │
│  Dimmer TRIAC│    { targetTemp }            │               │
│  OLED + CSV  │                              │  GET /api/data│ ←── Dashboard
│              │                              │  POST setTarget│ ←── Slider
│ Núcleo 0:    │                              └──────────────┘
│  WiFi HTTPS  │
└──────────────┘
```

## Despliegue Rápido

### 1. Backend + Dashboard (Vercel)

```bash
# Clona este repo
git clone https://github.com/TU-USUARIO/horno-isa-api.git
cd horno-isa-api

# Instala dependencias (solo para test local)
npm install

# Sube a Vercel
npx vercel --prod
```

O conecta el repo directamente en [vercel.com](https://vercel.com) → Import Project.

### 2. Firmware ESP32

1. Abre `firmware/horno_isa_esp32.ino` en Arduino IDE
2. Configura tu WiFi (`ssid`, `password`)
3. Pega tu URL de Vercel en `serverURL`
4. Instala las librerías: `Adafruit SSD1306`, `Adafruit GFX`, `MAX6675`
5. Compila y sube al ESP32

## Endpoints API

| Método | Ruta | Descripción |
|--------|------|-------------|
| POST | `/api/sync` | ESP32 envía temp/power/status, recibe targetTemp |
| GET | `/api/data` | Dashboard obtiene estado completo |
| POST | `/api/setTarget` | Dashboard cambia temperatura objetivo |
| POST | `/api/stop` | Apagado de emergencia (target → 0) |

## Pines ESP32

| Componente | Pin |
|-----------|-----|
| MAX6675 SCK | 19 |
| MAX6675 CS | 5 |
| MAX6675 SO | 23 |
| Zero-Cross | 34 |
| Dimmer TRIAC | 18 |
| OLED SDA | 21 (default) |
| OLED SCL | 22 (default) |

## Estructura

```
horno-isa-api/
├── api/
│   └── index.js          # Backend Express (Vercel Serverless)
├── public/
│   └── index.html        # Dashboard web (dark theme)
├── firmware/
│   └── horno_isa_esp32.ino  # Código ESP32 (referencia)
├── package.json
├── vercel.json
└── .gitignore
```
