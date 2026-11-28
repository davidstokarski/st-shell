#!/usr/bin/env bash
# Publishes every ST plug-in in one zip, as a GitHub release of st-shell (the family repo), so the website can offer
# "Download all plug-ins". Run it after releasing any plug-in (scripts/release.sh in its repo).
#
#   scripts/release-bundle.sh              # builds dist/ST-Plugins-<date>-macOS.zip + dist/ST-Plugins-macOS.zip, uploads
#   scripts/release-bundle.sh --no-upload
#
# Stable download link (what the website uses):
#   https://github.com/davidstokarski/st-shell/releases/latest/download/ST-Plugins-macOS.zip
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

OWNER="davidstokarski"
# repo  release-asset slug  product name (bundle names)
PLUGINS=(
    "st-eq      ST-EQ      ST EQ"
    "st-sat     ST-Sat     ST Sat"
    "st-dc      ST-DC      ST DC"
    "st-comp    ST-Comp    ST Comp"
    "st-split   ST-Split   ST Split"
    "st-sample  ST-Sample  ST Sample"
    "st-master  ST-Master  ST Master"
)
DATE="$(date +%Y.%m.%d)"
UPLOAD=1
[[ "${1:-}" == "--no-upload" ]] && UPLOAD=0

DIST="$ROOT/dist"
WORK="$DIST/bundle-work"
STAGE="$DIST/ST Plugins $DATE"
rm -rf "$WORK" "$STAGE"
mkdir -p "$WORK" "$STAGE"

VERSIONS=()
for entry in "${PLUGINS[@]}"; do
    read -r repo slug product <<< "$entry"
    tag="$(gh release view -R "$OWNER/$repo" --json tagName --jq .tagName)"
    echo "==> $product $tag"
    gh release download "$tag" -R "$OWNER/$repo" -p "$slug-macOS.zip" -D "$WORK/$repo" --clobber
    ditto -x -k "$WORK/$repo/$slug-macOS.zip" "$WORK/$repo/x"
    src="$(find "$WORK/$repo/x" -maxdepth 1 -mindepth 1 -type d | head -1)"
    ditto "$src/$product.vst3" "$STAGE/$product.vst3"
    ditto "$src/$product.component" "$STAGE/$product.component"
    VERSIONS+=("$product ${tag#v}")
done

{
    echo "Every ST plug-in, $DATE — macOS (Apple Silicon + Intel)"
    echo
    printf '  %s\n' "${VERSIONS[@]}"
    echo
    echo "1. Copy the .vst3 bundles to      ~/Library/Audio/Plug-Ins/VST3/"
    echo "2. Copy the .component bundles to ~/Library/Audio/Plug-Ins/Components/"
    echo "3. These builds are ad-hoc signed (not notarised). If macOS blocks them, run once in Terminal:"
    echo "     xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/ST* ~/Library/Audio/Plug-Ins/Components/ST*"
    echo "4. Restart your DAW (Logic: it rescans Audio Units on launch)."
} > "$STAGE/INSTALL.txt"

VERSIONED="$DIST/ST-Plugins-$DATE-macOS.zip"
STABLE="$DIST/ST-Plugins-macOS.zip"
rm -f "$VERSIONED" "$STABLE"
(cd "$DIST" && ditto -c -k --norsrc --noextattr --noqtn --keepParent "ST Plugins $DATE" "$VERSIONED")
cp "$VERSIONED" "$STABLE"
rm -rf "$WORK"
ls -lh "$VERSIONED" "$STABLE"

if [[ $UPLOAD -eq 1 ]]; then
    TAG="bundle-$DATE"
    NOTES="Every ST plug-in in one zip, universal macOS build (Apple Silicon + Intel): $(printf '%s, ' "${VERSIONS[@]}" | sed 's/, $//'). See INSTALL.txt inside."
    echo "==> Publishing GitHub release $TAG"
    if gh release view "$TAG" > /dev/null 2>&1; then
        gh release upload "$TAG" "$VERSIONED" "$STABLE" --clobber
        gh release edit "$TAG" --notes "$NOTES" --latest
    else
        gh release create "$TAG" "$VERSIONED" "$STABLE" --title "All ST plug-ins ($DATE)" --notes "$NOTES" --latest
    fi
fi
echo "Done."
