# Repository Guidelines

## Project Structure & Module Organization
Source code lives in `src/`: `heos_client.*` handles HEOS TCP connectivity, `mqtt_publisher.*` owns MQTT transport, and `main.cpp` wires both with signal handling. Unit tests and the fake HEOS server live under `tests/` using Catch2. Runtime assets sit in `docker/` (Mosquitto compose + config). Build metadata (`CMakeLists.txt`, `CMakePresets.json`, `vcpkg.json`) stays at the repo root so CI can discover it without extra flags.

## Build, Test, and Development Commands
Configure via `cmake --preset ninja-multi` (uses Ninja Multi-Config and `VCPKG_ROOT`). Build Debug binaries with `cmake --build --preset ninja-multi-debug` and Release with the `-release` preset. Run unit tests through `ctest --preset ninja-multi-tests`, which automatically builds the test target before execution. Use `docker compose -f docker/docker-compose.yml up -d` to launch Mosquitto for local/system tests.

## Coding Style & Naming Conventions
Write modern C++20 with the standard library plus Boost.Asio/MQTT/JSON and fmt. Prefer RAII, `std::unique_ptr`/`std::shared_ptr` as needed, and asynchronous patterns via `boost::asio::strand`. Keep log messages terse by routing them through `fmt::print`. File-level naming and every symbol (types, methods, functions, variables, aliases, tests) must use snake_case so identifiers line up with their filenames; avoid CamelCase entirely, and keep constants SCREAMING_SNAKE. Represent timeouts/durations with `std::chrono` types—never raw integers. Use 4-space indentation, `#pragma once` guards, and limit new dependencies to what `vcpkg.json` declares. When adding formatting or linting helpers, wire them into CMake so CI can run them.

## Testing Guidelines
Unit tests rely on Catch2 (`heos_client_tests`) plus a Boost.Asio fake HEOS server; mimic that pattern when covering new networking logic. Write deterministic tests that avoid touching real HEOS hardware or MQTT brokers—script responses and assert ordering/timeouts. Name tests descriptively (`TEST_CASE("heos_client reconnects", "[heos-client]")`). Always run `ctest --preset ninja-multi-tests` before pushing, and attach new suites to CTest so `ctest` picks them up automatically. Aim for coverage of reconnection paths, shutdown behavior, and error handling.

## Commit & Pull Request Guidelines
Use short, imperative commit messages ("Add reconnect unit test"), referencing issues like `[#42]` when applicable. Squash fixups locally; avoid force pushes on shared branches unless coordinating with reviewers. PRs should summarize behavior changes, list new commands/configs, and mention test evidence (command snippets or logs). Include screenshots only when UI or monitoring artifacts change. Link to Mosquitto/docker docs for infra tweaks and tag reviewers responsible for HEOS or MQTT components.
