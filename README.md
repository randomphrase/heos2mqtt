# heos2mqtt

heos2mqtt bridges Denon/Marantz HEOS CLI output to MQTT so that downstream services can react to raw messages.

## Requirements
- CMake 3.24+
- Ninja (Multi-Config)
- vcpkg (manifest mode); set `VCPKG_ROOT` or pass `-DVCPKG_ROOT` when configuring
- At least clang 17 (the default macOS clang++ on Tahoe or later) or GCC equivalent on recent linux

On Debian:
- autotools, libtool (both needed to build libbacktrace)
- liburing-dev


## Build & Test
```bash
cmake --preset ninja-multi
cmake --build --preset ninja-multi-debug
ctest --preset ninja-multi-tests
```

## Run the service
```bash
./build/ninja-multi/Debug/heos2mqtt \
  --heos-host 192.168.1.50 --heos-port 1255 \
  --mqtt-host localhost --mqtt-port 1883 \
  --base-topic heos
```

The service connects to the HEOS CLI (default port 1255) and publishes JSON payloads such as `{"raw":"heos.message","ts":"2024-04-01T12:00:00Z"}` to `heos/raw`. Use `fmt` logging on stdout/stderr for visibility.

## Local Mosquitto broker
```
cd docker
docker compose up -d
```
The container exposes `localhost:1883` with anonymous access for local testing.

## Project layout
```
src/               # heos_client, mqtt_publisher, main entry point
tests/             # Catch2-based unit tests with fake HEOS server
CMakeLists.txt     # build graph
CMakePresets.json  # Ninja Multi-Config presets
docker/            # Mosquitto docker-compose and config
```
