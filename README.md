# Landslide-WSN

A fault-tolerant wireless sensor network for landslide early warning, built on ESP32 microcontrollers.

## Architecture
- Custom mesh protocol over ESP-NOW (lightweight, no TCP stack)
- Self-healing topology — automatic node dropout detection and rejoin
- Soil moisture and accelerometer sensors as landslide indicators
- Gateway node with Blynk cloud dashboard and local HTTP dashboard

## Why ESP-NOW over painlessMesh
painlessMesh uses TCP under the hood which caused heap allocation failures on constrained ESP32 hardware. We implemented a custom application-layer mesh protocol over ESP-NOW, Espressif's native peer-to-peer protocol, achieving lower memory footprint and better stability.

## Nodes
- Gateway (Janus): Central hub, Blynk integration, local web server
- Sensor Node 1 (Fissure): Soil moisture sensing
- Sensor Node 2: Soil moisture sensing
- Sensor Node 3 (pending): Accelerometer (MPU6050)

## Tech Stack
ESP32, ESP-NOW, Arduino framework, Blynk IoT, PlatformIO
