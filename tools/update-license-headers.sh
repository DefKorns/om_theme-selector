#!/bin/sh
# Stamps the canonical GPL header (tools/license-header.txt) into the top of
# every project script, replacing only the "Copyright (c) ... gnu.org" block.
# Everything before it (shebang, shellcheck disable comments) and after it
# (source/script_init lines, code) is left untouched.
#
# Usage:
#   sh tools/update-license-headers.sh [YEAR]
# YEAR defaults to the current year.
set -eu

script_dir="$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)"
repo_root="$(CDPATH='' cd -- "$script_dir/.." && pwd)"
header_file="$script_dir/license-header.txt"
year="${1:-$(date +%Y)}"

if [ ! -f "$header_file" ]; then
  echo "missing $header_file" >&2
  exit 1
fi

files=$(
  {
    [ -f "$repo_root/mod/install" ] && echo "$repo_root/mod/install"
    [ -f "$repo_root/mod/uninstall" ] && echo "$repo_root/mod/uninstall"
    find "$repo_root/mod" -type f -name 'om_*'
  } | sort -u | xargs grep -lE '^#!/bin/(sh|bash)' 2>/dev/null
)

updated=0
for f in $files; do
  if grep -q '^#[[:space:]]*Copyright (c)' "$f"; then
    tmp="$f.tmp.$$"
    awk -v year="$year" -v hdrfile="$header_file" '
      BEGIN {
        n = 0
        while ((getline line < hdrfile) > 0) hdr[n++] = line
        close(hdrfile)
      }
      skip_blank {
        skip_blank = 0
        if ($0 ~ /^#[ \t]*$/) next
      }
      !started && /^#[ \t]*Copyright \(c\)/ {
        started = 1
        inblock = 1
        for (i = 0; i < n; i++) {
          l = hdr[i]
          gsub(/\{YEAR\}/, year, l)
          print l
        }
        next
      }
      inblock {
        if ($0 ~ /gnu\.org\/licenses/) {
          inblock = 0
          skip_blank = 1
        }
        next
      }
      { print }
    ' "$f" >"$tmp"
    mv "$tmp" "$f"
    updated=$((updated + 1))
    echo "updated: ${f#"$repo_root"/}"
  fi
done

echo "$updated file(s) updated to copyright year $year"
