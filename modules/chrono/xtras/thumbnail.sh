#!/bin/sh
gStorage="$(hakchi findGameStorage)"
find "$gStorage" -path "*_small.png*" | while read file; do
 [ -f "$file" ] && cp "$file" "$file"_backup && cp "/var/lib/hakchi/rootfs/usr/share/themes/snes/ghost/xtras/thumb.png" "$file"
 done

