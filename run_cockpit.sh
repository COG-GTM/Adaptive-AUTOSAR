#!/usr/bin/env bash
# One-command startup of the Adaptive AUTOSAR simulation with the ECU cockpit dashboard.
set -euo pipefail

cd "$(dirname "$0")"

export DASHBOARD_PORT="${DASHBOARD_PORT:-8088}"
export DASHBOARD_ROOT="${DASHBOARD_ROOT:-./web}"

cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config Debug -j"$(nproc)"

echo "ECU cockpit dashboard: http://127.0.0.1:${DASHBOARD_PORT}"

exec ./build/bin/adaptive_autosar \
    ./configuration/execution_manifest.arxml \
    ./configuration/extended_vehicle_manifest.arxml \
    ./configuration/diagnostic_manager_manifest.arxml \
    ./configuration/health_monitoring_manifest.arxml
