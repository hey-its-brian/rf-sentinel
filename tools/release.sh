#!/usr/bin/env bash
# Cut an RF Sentinel release from main.
#
#   tools/release.sh 0.2.0 [notes.md]
#
# Sets the version in CMakeLists.txt (node) and host-cyd/platformio.ini (host),
# commits, tags vX.Y.Z and pushes. GitHub Actions (.github/workflows/release.yml)
# then builds both images, publishes the release and deploys the web flasher.
# Release notes: put them in docs/releases/vX.Y.Z.md before running, or pass a file.
set -euo pipefail

ver="${1:?usage: $0 X.Y.Z [notes.md]}"
root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$root"

[[ "$ver" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || { echo "version must be X.Y.Z"; exit 1; }
[[ "$(git branch --show-current)" == "main" ]] || { echo "release from main"; exit 1; }
[[ -z "$(git status --porcelain)" ]] || { echo "working tree not clean"; exit 1; }

if [[ -n "${2:-}" ]]; then mkdir -p docs/releases; cp "$2" "docs/releases/v$ver.md"; fi
[[ -f "docs/releases/v$ver.md" ]] || { echo "no notes: docs/releases/v$ver.md"; exit 1; }

sed -i '' -E "s/set\(PROJECT_VER \"[^\"]*\"\)/set(PROJECT_VER \"$ver\")/" CMakeLists.txt
sed -i '' -E "s/#define RFS_FW_VERSION \"[^\"]*\"/#define RFS_FW_VERSION \"$ver\"/" main/proto.h
sed -i '' -E "s/-DRFS_HOST_VERSION=\\\\\"[^\"]*\\\\\"/-DRFS_HOST_VERSION=\\\\\"$ver\\\\\"/" host-cyd/platformio.ini
git add -A
git diff --cached --quiet || git commit -qm "v$ver"

git push -q origin main
git tag -a "v$ver" -m "v$ver"
git push -q origin "v$ver"
echo "tagged v$ver; watch https://github.com/hey-its-brian/rf-sentinel/actions"
