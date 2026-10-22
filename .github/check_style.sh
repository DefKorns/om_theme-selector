#!/bin/sh
# check_style.sh <base>: the lines added since <base> follow .editorconfig
base="${1:-origin/master}"
failed=0

error() {
  echo "::error file=$1::$2"
  failed=1
}

git diff --check "$base...HEAD" -- . ':!*.md' || failed=1

files=$(git diff --name-only --diff-filter=ACMR "$base...HEAD")
for file in $files; do
  case "$file" in
  *.png | *.ttf | *.hmod | *.xz | *.pem) continue ;;
  esac
  added=$(git diff -U0 "$base...HEAD" -- "$file" | grep '^+' | grep -v '^+++' || true)
  [ -n "$added" ] || continue
  printf '%s\n' "$added" | grep -q "$(printf '\r')" && error "$file" "CRLF line endings; use LF"
  case "$file" in
  Makefile | *.mk | *.md | .gitmodules) ;;
  *) printf '%s\n' "$added" | grep -q "^+$(printf '\t')" && error "$file" "indented with tabs; use spaces (see .editorconfig)" ;;
  esac
  [ -n "$(tail -c1 "$file")" ] && error "$file" "missing newline at the end of the file"
done

exit "$failed"
