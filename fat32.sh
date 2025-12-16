#!/bin/bash
set -xue

DIR=fsroot
DISK_IMAGE=fat32.img
SECTORS_PER_CLUSTER=8
SIZE_MB=$(( ( $(du -sb "$DIR" | cut -f1) + 8*1024*1024 ) / 1024 / 1024 ))

dd if=/dev/zero of=$DISK_IMAGE bs=1M count=$SIZE_MB
mkfs.fat -F 32 -s $SECTORS_PER_CLUSTER $DISK_IMAGE
mcopy -i $DISK_IMAGE -s $DIR/* ::/

# Verify
mdir -i $DISK_IMAGE ::