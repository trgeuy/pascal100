#!/bin/sh
# Build pascal-iii-buildkit.zip from the files in this repository:
#   files/*                              the machine file, the BIOS and boot sources, README.txt
#   HOWTO-Build-the-System.md            the guide
#   ../../disks/compupro-disk1/cpm22/CPM.DSK  the CP/M starting disk
# The zip unpacks to a folder named pascal-iii-buildkit. Run this script after you change any of these
# files, and commit pascal-iii-buildkit.zip with the change.
#
# The two Pascal-100 disks are not in the zip. README.txt tells the reader where to get them.
set -eu

here=$(cd "$(dirname "$0")" && pwd)
disk="$here/../../disks/compupro-disk1/cpm22/CPM.DSK"
stamp=202611180000          # a fixed time, so that the same files give the same zip

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
name=pascal-iii-buildkit
kit="$tmp/$name"
mkdir "$kit"

cp "$here"/files/* "$kit"/
cp "$here/HOWTO-Build-the-System.md" "$kit"/
cp "$disk" "$kit/CPM.DSK"

chmod 644 "$kit"/*
chmod 444 "$kit/CPM.DSK"    # the guide's Step 1.2 makes the copies writable
touch -t "$stamp" "$kit" "$kit"/*

rm -f "$here/$name.zip"
(cd "$tmp" && find "$name" | LC_ALL=C sort | zip -q -X "$here/$name.zip" -@)
unzip -l "$here/$name.zip"
