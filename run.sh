#!/bin/bash
set -xue

# QEMU file path
QEMU=qemu-system-riscv32
QEMU_SMP=4

CC=$(which riscv64-unknown-elf-gcc)
CPP=$(which riscv64-unknown-elf-g++)
CFLAGS="-march=rv32imac_zicsr -mabi=ilp32 -std=c99 -nostdlib -nostdinc -g3 -O3 -Wall -Werror -fno-builtin"
CCFLAGS="-march=rv32imac_zicsr -mabi=ilp32 -std=c++20 -nostdlib -nostdinc -g3 -O3 -Wall -Werror -fno-builtin -fno-exceptions -fno-rtti -ffreestanding"
CDIR=src
ODIR=build

mkdir -p "$ODIR"

echo "Building all .c files under $CDIR..."
for SRC in $CDIR/*.c; do
    [[ -f "$SRC" ]] || continue
    OBJ="$ODIR/$(basename "$SRC" .c).o"
    echo "  → $SRC → $OBJ"
    $CC $CFLAGS -c "$SRC" -o "$OBJ"
done

echo "Building all .cc files under $CDIR..."
for SRC in $CDIR/*.cc; do
    [[ -f "$SRC" ]] || continue
    OBJ="$ODIR/$(basename "$SRC" .cc).o"
    echo "  → $SRC → $OBJ"
    $CPP $CCFLAGS -c "$SRC" -o "$OBJ"
done

echo "Building all assembly (.S/.s) files under $CDIR..."
for SRC in $CDIR/*.S $CDIR/*.s; do
    [[ -f "$SRC" ]] || continue
    OBJ="$ODIR/$(basename "$SRC").o"
    echo "  → $SRC → $OBJ"
    $CC $CFLAGS -c "$SRC" -o "$OBJ"
done

echo "Linking all .o files under $ODIR..."
$CPP $CCFLAGS -Wl,-T$CDIR/kernel.ld -Wl,-Map=$ODIR/kernel.map $ODIR/*.o -o $ODIR/kernel.elf

# Run QEMU
$QEMU -machine virt -bios default -nographic -serial mon:stdio --no-reboot -smp $QEMU_SMP -kernel $ODIR/kernel.elf