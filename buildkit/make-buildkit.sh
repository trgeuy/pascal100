#!/bin/sh
# Build buildkit.zip from the files in this repository:
#   files/*                              the machine file, the BIOS and boot sources, README.txt
#   HOWTO-Build-the-System.md            the guide
#   ../disks/compupro-disk1/cpm22/CPM.DSK  the CP/M starting disk
# The zip unpacks to a folder named buildkit. Run this script after you change any of these
# files, and commit buildkit.zip with the change.
#
# The two Pascal-100 disks are not in the zip. README.txt tells the reader where to get them.
set -eu

here=$(cd "$(dirname "$0")" && pwd)
disk="$here/../disks/compupro-disk1/cpm22/CPM.DSK"
stamp=202611180000          # a fixed time, so that the same files give the same zip

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
kit="$tmp/buildkit"
mkdir "$kit"

cp "$here"/files/* "$kit"/
cp "$here/HOWTO-Build-the-System.md" "$kit"/
cp "$disk" "$kit/CPM.DSK"

chmod 644 "$kit"/*
chmod 444 "$kit/CPM.DSK"    # the guide's Step 1.2 makes the copies writable
touch -t "$stamp" "$kit" "$kit"/*

rm -f "$here/buildkit.zip"
(cd "$tmp" && find buildkit | LC_ALL=C sort | zip -q -X "$here/buildkit.zip" -@)
unzip -l "$here/buildkit.zip"
