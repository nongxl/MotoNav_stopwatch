#!/bin/sh
set -eu
taskRoot=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
taskOutput="$taskRoot/.amap-only-tests"
mkdir -p "$taskOutput"
swiftc "$taskRoot/MotoNav/MapGeo.swift" "$taskRoot/MotoNav/RasterProcessing.swift" "$taskRoot/MotoNav/SatelliteRaster.swift" "$taskRoot/MotoNav/MapDefaults.swift" "$taskRoot/tests/amap-only/main.swift" -o "$taskOutput/amap-only"
"$taskOutput/amap-only"
