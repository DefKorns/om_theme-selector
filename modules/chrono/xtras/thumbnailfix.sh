#!/bin/sh
gStorage="$(hakchi findGameStorage)"
find "$gStorage" -path "*_small.png*" | while read file; do
 [ -f "$file"_backup ] && cp "$file"_backup "$file" && rm "$file"_backup
 done

