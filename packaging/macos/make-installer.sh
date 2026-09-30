#!/usr/bin/env bash
# Builds <Product>-<version>-macOS.pkg (and a .zip of the raw plugins) from a finished build.
# PRODUCT=Spark (default) or PRODUCT=SparkLead.
#
# Unsigned by default (ad-hoc signatures). To produce a signed + notarised installer set:
#   APP_SIGN_IDENTITY        e.g. "Developer ID Application: Your Name (TEAMID)"
#   INSTALLER_SIGN_IDENTITY  e.g. "Developer ID Installer: Your Name (TEAMID)"
#   NOTARY_APPLE_ID, NOTARY_TEAM_ID, NOTARY_PASSWORD (app-specific password)
set -Eeuo pipefail
trap 'echo "::error file=packaging/macos/make-installer.sh,line=$LINENO::failed: $BASH_COMMAND"' ERR

VERSION="${1:-1.0.0}"
BUILD_DIR="${BUILD_DIR:-build}"
CONFIG="${CONFIG:-Release}"
OUT_DIR="${OUT_DIR:-dist}"
HERE="$(cd "$(dirname "$0")" && pwd)"
PRODUCT="${PRODUCT:-Spark}"
PRODUCT_ID="$(echo "$PRODUCT" | tr '[:upper:]' '[:lower:]')"
TEMPLATES="$HERE"
[[ -d "$HERE/$PRODUCT_ID" ]] && TEMPLATES="$HERE/$PRODUCT_ID"

STAGE="$(mktemp -d)"
trap 'rm -rf "$STAGE"' EXIT
mkdir -p "$OUT_DIR" "$STAGE/pkgs" "$STAGE/resources"

I="$BUILD_DIR/${PRODUCT}_artefacts/$CONFIG"

sign_bundle() {
    local bundle="$1"
    if [[ -n "${APP_SIGN_IDENTITY:-}" ]]; then
        codesign --force --deep --timestamp --options runtime --preserve-metadata=entitlements \
                 --sign "$APP_SIGN_IDENTITY" "$bundle"
    else
        codesign --force --deep --preserve-metadata=entitlements --sign - "$bundle"
    fi
    codesign --verify --deep --strict "$bundle"
}

# component id | install folder | bundles...
make_component() {
    local id="$1" dest="$2"; shift 2
    local root="$STAGE/root-$id"
    mkdir -p "$root/$dest"
    for b in "$@"; do
        [[ -e "$b" ]] || { echo "::error::Missing build product: $b"; ls -R "$BUILD_DIR"/*_artefacts | head -50; exit 1; }
        ditto "$b" "$root/$dest/$(basename "$b")"
        sign_bundle "$root/$dest/$(basename "$b")"
    done
    # Stop Installer "relocating" bundles to wherever an older copy lives.
    pkgbuild --analyze --root "$root" "$STAGE/$id.plist" >/dev/null
    local i=0
    while /usr/libexec/PlistBuddy -c "Print :$i:RootRelativeBundlePath" "$STAGE/$id.plist" >/dev/null 2>&1; do
        /usr/libexec/PlistBuddy -c "Delete :$i:BundleIsRelocatable" "$STAGE/$id.plist" >/dev/null 2>&1 || true
        /usr/libexec/PlistBuddy -c "Add :$i:BundleIsRelocatable bool false" "$STAGE/$id.plist" \
            || echo "::warning::Couldn't mark bundle $i in $id as non-relocatable"
        i=$((i + 1))
    done
    pkgbuild --root "$root" --component-plist "$STAGE/$id.plist" \
             --identifier "com.sparkaudio.$PRODUCT_ID.$id" --version "$VERSION" \
             --install-location / "$STAGE/pkgs/$id.pkg"
}

make_component vst3 "Library/Audio/Plug-Ins/VST3" "$I/VST3/$PRODUCT.vst3"
make_component au   "Library/Audio/Plug-Ins/Components" "$I/AU/$PRODUCT.component"
make_component apps "Applications" "$I/Standalone/$PRODUCT.app"

sed "s/@VERSION@/$VERSION/g" "$TEMPLATES/welcome.html" > "$STAGE/resources/welcome.html"
cp "$TEMPLATES/conclusion.html" "$STAGE/resources/conclusion.html"
sed "s/@VERSION@/$VERSION/g" "$TEMPLATES/distribution.xml" > "$STAGE/distribution.xml"

PKG="$OUT_DIR/$PRODUCT-$VERSION-macOS.pkg"
if [[ -n "${INSTALLER_SIGN_IDENTITY:-}" ]]; then
    productbuild --distribution "$STAGE/distribution.xml" --package-path "$STAGE/pkgs" \
                 --resources "$STAGE/resources" --sign "$INSTALLER_SIGN_IDENTITY" "$PKG"
else
    productbuild --distribution "$STAGE/distribution.xml" --package-path "$STAGE/pkgs" \
                 --resources "$STAGE/resources" "$PKG"
fi

if [[ -n "${NOTARY_APPLE_ID:-}" && -n "${INSTALLER_SIGN_IDENTITY:-}" ]]; then
    xcrun notarytool submit "$PKG" --apple-id "$NOTARY_APPLE_ID" --team-id "$NOTARY_TEAM_ID" \
                            --password "$NOTARY_PASSWORD" --wait
    xcrun stapler staple "$PKG"
fi

# Plain zip for people who prefer to drag plugins in themselves
ZIPROOT="$STAGE/$PRODUCT-$VERSION-macOS"
mkdir -p "$ZIPROOT/VST3" "$ZIPROOT/AU" "$ZIPROOT/Apps"
ditto "$STAGE/root-vst3/Library/Audio/Plug-Ins/VST3" "$ZIPROOT/VST3"
ditto "$STAGE/root-au/Library/Audio/Plug-Ins/Components" "$ZIPROOT/AU"
ditto "$STAGE/root-apps/Applications" "$ZIPROOT/Apps"
(cd "$STAGE" && ditto -c -k --keepParent "$PRODUCT-$VERSION-macOS" "$PRODUCT-$VERSION-macOS-plugins.zip")
mv "$STAGE/$PRODUCT-$VERSION-macOS-plugins.zip" "$OUT_DIR/"

pkgutil --payload-files "$PKG" > "$STAGE/payload.txt" 2>&1 || true
grep -E "\.(vst3|component|app)$" "$STAGE/payload.txt" || true
ls -la "$OUT_DIR"
