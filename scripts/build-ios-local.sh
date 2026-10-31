#!/bin/bash
set -euo pipefail
taskRoot="$(cd -- "$(dirname -- "$0")/.." && pwd)"
exec bash "$taskRoot/build-ios.sh" "$@"
