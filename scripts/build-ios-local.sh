#!/bin/bash
set -euo pipefail
taskRoot="$(cd -- "$(dirname -- "$0")/.." && pwd)"
taskOutput="$taskRoot/output"
mkdir -p "$taskOutput"
command -v xcodebuild >/dev/null || { echo '需要在安装了 Xcode 的 Mac 上运行';exit 1; }
command -v pod >/dev/null || { echo '请先安装 CocoaPods 1.16.2';exit 1; }
cd "$taskRoot/ios"
pod install --deployment
cd "$taskRoot"
python3 ios/tests/webmotor/check.py | tee "$taskOutput/core-tests.log"
swiftc ios/MotoNav/ArrivalPolicy.swift ios/tests/arrival/main.swift -o "$taskOutput/arrival-test"
"$taskOutput/arrival-test"
swiftc ios/MotoNav/ArrivalPolicy.swift ios/MotoNav/NaviSessionCallbacks.swift ios/tests/arrival-relay/SDKStubs.swift ios/tests/arrival-relay/main.swift -o "$taskOutput/relay-test"
"$taskOutput/relay-test"
swiftc -O ios/MotoNav/MapGeo.swift ios/MotoNav/RasterProcessing.swift ios/MotoNav/MapStyles.swift ios/tests/four-styles/main.swift -o "$taskOutput/styles-test"
"$taskOutput/styles-test"
xcodebuild -workspace ios/MotoNav.xcworkspace -scheme MotoNav -destination 'generic/platform=iOS' -configuration Release -derivedDataPath "$taskOutput/Derived" CODE_SIGNING_ALLOWED=NO CODE_SIGNING_REQUIRED=NO DEVELOPMENT_TEAM= build | tee "$taskOutput/build-ios.log"
taskApp="$taskOutput/Derived/Build/Products/Release-iphoneos/MotoNav.app"
test "$(/usr/libexec/PlistBuddy -c 'Print CFBundleShortVersionString' "$taskApp/Info.plist")" = '0.15.0'
test -f "$taskApp/PlugIns/MotoNavShare.appex/MotoNavShare"
taskStage="$(mktemp -d "$taskOutput/stage.XXXXXX")"
mkdir -p "$taskStage/Payload"
ditto "$taskApp" "$taskStage/Payload/MotoNav.app"
(cd "$taskStage" && zip -qry "$taskOutput/MotoNav-0.15.0-unsigned.ipa" Payload)
(cd "$taskOutput" && shasum -a 256 MotoNav-0.15.0-unsigned.ipa > SHA256SUMS.txt)
echo "已生成：$taskOutput/MotoNav-0.15.0-unsigned.ipa（仍需用户签名安装）"
