# 💓 Ingeniería Inversa: Decodificación de Protocolo BLE (COOSPO H6)

Proyecto de análisis y extracción de datos biométricos mediante **Ingeniería Inversa** al monitor cardíaco COOSPO H6. El sistema utiliza un **ESP32** como cliente central para capturar pulso y variabilidad cardíaca, validado mediante **Sniffing de hardware** y análisis de software.

---

## 🚀 Tecnologías y Herramientas Utilizadas

| Módulo | Tecnología / Herramienta |
|-------|-----------|
| Monitor Biométrico | COOSPO H6 (Sensor de Pecho) |
| Microcontrolador | ESP32 (Cliente BLE en C++) |
| Sniffer de Hardware | nRF51822 (Captura de Advertising) |
| Ingeniería de Software | Android Nativo (Kotlin) |
| Estándar de Red | Bluetooth Low Energy (Heart Rate Service) |

---

## 🧠 Arquitectura del Flujo de Trabajo

### ✅ 1. Auditoría Física (Sniffing)
- **Componentes:** nRF51822 + Wireshark.
- **Logro:** Captura de paquetes de anuncio y validación de MAC `DF:3D:29:04:15:CF`.
- **Limitación:** Solo se capturó el tráfico de anuncio; la lógica de datos se derivó del análisis de software.

---

### ✅ 2. Ingeniería de Software (Análisis de App)
Basado en el análisis de lógica Kotlin para entender el **Flags Byte**:
1. **Bit 0:** Determina si el pulso es UINT8 o UINT16.
2. **Bit 4:** Indica presencia de intervalos RR.
3. **Cálculo RR:** Conversión de unidades 1/1024 a milisegundos.

---

### ✅ 3. Implementación ESP32 (Cliente BLE)

Flujo:
1. **Escaneo:** Filtra por nombre `H6M 08012`.
2. **Conexión:** Se vincula al Service `0x180D`.
3. **Suscripción:** Activa notificaciones en la Characteristic `0x2A37`.
4. **Decodificación:** Procesa el payload en tiempo real (BPM + RR).
5. **Batería:** Lectura síncrona cada 5 segundos del Service `0x180F`.

---

## 📊 Estructura de la Trama (Payload 0x2A37)

| Posición | Campo | Descripción |
|----------|-------|-------------|
| Byte 0 | Flags | Define qué datos vienen en el paquete |
| Byte 1 | BPM | Frecuencia cardíaca (8-bit) |
| Byte 2-N | RR-Intervals | Pares de bytes con la variabilidad (16-bit) |

---

## 📂 Estructura del Repositorio

/Documentacion → Reporte técnico LaTeX + Capturas Wireshark
/Software-Analysis → Lógica de decodificación Kotlin
/final.ino → Firmware para ESP32
/README.md → Guía del proyecto

---

## ⚙️ Instalación y Uso

### 🛠️ ESP32
- Abrir `final.ino` en Arduino IDE.
- Instalar soporte para placas ESP32.
- No requiere librerías externas (usa `BLEDevice.h` nativa).
- Cargar código y abrir Monitor Serial (115200 bps).

### 🔍 Validación con Sniffer
- Conectar nRF51822.
- Abrir Wireshark con el plugin de Nordic.
- Filtrar por la dirección MAC del monitor para observar el proceso de Advertising.

---

## 👩‍💻 Autores

| Integrante | Universidad de Cuenca |
|-----------|-----------------------|
| Evelyn Nayeli Cabrera Jara | Facultad de Ingeniería |
| John Rogelio Molina León | Ingeniería en Electrónica y Telecomunicaciones |
