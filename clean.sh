#!/usr/bin/env bash
DIR="build"

if [ -d "$DIR" ]; then
    echo "Removing directory "$DIR" ..."
    rm -rf "$DIR"
    echo "Done."
else
    echo "Directory "$DIR" does not exist, nothing to do."
fi
