#!/bin/bash
set -euo pipefail
ROOT="$(cd -- "$(dirname -- "$0")/.." && pwd)"
DERIVED="${MOTONAV_DERIVED_DATA:-$(mktemp -d /tmp/MotoNavRelease.XXXXXX)}"
OUT="$ROOT/output/ios"
mkdir -p "$OUT"
xcodebuild -workspace "$ROOT/ios/MotoNav.xcworkspace" -scheme MotoNav \
  -destination 'generic/platform=iOS' -configuration Release \
  -derivedDataPath "$DERIVED" CODE_SIGNING_ALLOWED=NO CODE_SIGNING_REQUIRED=NO DEVELOPMENT_TEAM= build
APP="$DERIVED/Build/Products/Release-iphoneos/MotoNav.app"
VERSION=$(/usr/libexec/PlistBuddy -c 'Print CFBundleShortVersionString' "$APP/Info.plist")
BUILD=$(/usr/libexec/PlistBuddy -c 'Print CFBundleVersion' "$APP/Info.plist")
EXT_VERSION=$(/usr/libexec/PlistBuddy -c 'Print CFBundleShortVersionString' "$APP/PlugIns/MotoNavShare.appex/Info.plist")
EXT_BUILD=$(/usr/libexec/PlistBuddy -c 'Print CFBundleVersion' "$APP/PlugIns/MotoNavShare.appex/Info.plist")
if [[ ! "$VERSION" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ || ! "$BUILD" =~ ^[0-9]+$ || "$VERSION" != "$EXT_VERSION" || "$BUILD" != "$EXT_BUILD" ]]; then
  echo '应用与分享扩展版本不一致，或版本格式不正确。' >&2
  exit 1
fi
STAGE=$(mktemp -d /tmp/MotoNavPayload.XXXXXX)
trap 'rm -rf "$STAGE"' EXIT
mkdir -p "$STAGE/Payload"
ditto "$APP" "$STAGE/Payload/MotoNav.app"
find "$STAGE" -name _CodeSignature -type d -prune -exec rm -rf {} +
find "$STAGE" -name embedded.mobileprovision -delete
xcrun strip -S "$STAGE/Payload/MotoNav.app/MotoNav"
xcrun strip -S "$STAGE/Payload/MotoNav.app/PlugIns/MotoNavShare.appex/MotoNavShare"
IPA="$OUT/MotoNav-$VERSION-build$BUILD-unsigned.ipa"
rm -f "$IPA"
(cd "$STAGE" && /usr/bin/zip -qry "$IPA" Payload)
(cd "$OUT" && shasum -a 256 "$(basename "$IPA")" > SHA256SUMS.txt)
echo "$IPA"
