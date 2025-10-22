#!/bin/bash
set -xue

# QEMU file path
QEMU=qemu-system-riscv32

CC=$(which riscv64-unknown-elf-gcc)
CPP=$(which riscv64-unknown-elf-g++)
CFLAGS="-march=rv32imac -mabi=ilp32 -std=c99 -nostdlib -nostdinc -g3 -O3 -Wall -Werror -fno-builtin"
CCFLAGS="-march=rv32imac -mabi=ilp32 -std=c++20 -nostdlib -nostdinc -g3 -O3 -Wall -Werror -fno-builtin -fno-exceptions -fno-rtti -ffreestanding"
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

echo "Building all .cc files under $CDIR..."

# Compile each .cc file separately
for SRC in $CDIR/*.cc; do
    # Skip if no .cc files are found
    [[ -f "$SRC" ]] || continue

    OBJ="$ODIR/$(basename "$SRC" .cc).o"
    echo "  → $SRC → $OBJ"
    $CPP $CCFLAGS -c "$SRC" -o "$OBJ"
done

echo "Linking all .o files under $ODIR..."

# Link the object files
$CPP $CCFLAGS -Wl,-T$CDIR/kernel.ld -Wl,-Map=$ODIR/kernel.map build/*.o -o $ODIR/kernel.elf

# Start QEMU
$QEMU -machine virt -bios default -nographic -serial mon:stdio --no-reboot -kernel $ODIR/kernel.elf