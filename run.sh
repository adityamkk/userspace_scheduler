#!/bin/bash
set -xue

# QEMU file path
QEMU=qemu-system-riscv32

CC=$(which riscv64-unknown-elf-gcc)
CFLAGS="-march=rv32imac -mabi=ilp32 -std=c99 -nostdlib -nostdinc -g3 -O3 -Wall -Werror -fno-builtin"
CDIR=src
ODIR=build

# Make the build directory if it doesn't already exist
mkdir -p "$ODIR"

echo "Building all .c files under $CDIR..."

# Compile each .c file separately
for SRC in $CDIR/*.c; do
    # Skip if no .c files are found
    [[ -f "$SRC" ]] || continue

    OBJ="$ODIR/$(basename "$SRC" .c).o"
    echo "  → $SRC → $OBJ"
    $CC $CFLAGS -c "$SRC" -o "$OBJ"
done

# Link the object files
$CC $CFLAGS -Wl,-T$CDIR/kernel.ld -Wl,-Map=$ODIR/kernel.map build/*.o -o $ODIR/kernel.elf

# Start QEMU
$QEMU -machine virt -bios default -nographic -serial mon:stdio --no-reboot -kernel $ODIR/kernel.elf