#!/usr/bin/env bash
# Publish this folder as a GitHub repo under the signed-in gh user.
# Usage: ./publish.sh <repo-name> <public|private>
set -euo pipefail

REPO="${1:-MEPA_Hand_Controller}"
VIS="${2:-private}"

if [ -f secrets.h ] && ! grep -q '^secrets.h$' .gitignore; then
  echo "Refusing to publish: secrets.h is not in .gitignore" >&2
  exit 1
fi

git init -q
git add -A
git -c user.name="Saka Adetayo Muhammed" \
    -c user.email="${GIT_EMAIL:-adetayosaka045@gmail.com}" \
    commit -q -m "Initial commit: MEPA hand controller"

gh repo create "$REPO" "--$VIS" --source=. --remote=origin --push
echo "Done. Open:  gh repo view --web"
