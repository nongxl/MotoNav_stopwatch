#!/bin/bash
set -euo pipefail
taskRoot="$(cd -- "$(dirname -- "$0")" && pwd)"
cd "$taskRoot"
test "$(uname -s)" = Darwin || { echo '请在安装 Xcode 的 Mac 上运行'; exit 1; }
command -v pod >/dev/null || { echo '请先安装 CocoaPods 1.16.2'; exit 1; }
(cd ios && pod _1.16.2_ install --deployment)
python3 ios/tests/webmotor/check.py
bash scripts/build-ios-ipa.sh
