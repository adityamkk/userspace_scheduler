#!/bin/bash
set -xue

# QEMU file path
QEMU=qemu-system-riscv32
QEMU_SMP=4

CC=$(which riscv64-unknown-elf-gcc)
CPP=$(which riscv64-unknown-elf-g++)
CFLAGS="-march=rv32imac_zicsr -mabi=ilp32 -std=c99 -nostdlib -nostdinc -g3 -O3 -Wall -Werror -fno-builtin -ffixed-tp"
CCFLAGS="-march=rv32imac_zicsr -mabi=ilp32 -std=c++20 -nostdlib -nostdinc -g3 -O3 -Wall -Werror -fno-builtin -fno-exceptions -fno-rtti -ffreestanding -ffixed-tp"
CDIR=src
ODIR=build

mkdir -p "$ODIR"

# Helper to compile files found recursively under $CDIR and mirror the
# directory structure under $ODIR to avoid name collisions.
compile_c_files() {
    echo "Building all .c files under $CDIR (recursive)..."
    find "$CDIR" -type f -name '*.c' -print0 | while IFS= read -r -d '' SRC; do
        REL=${SRC#${CDIR}/}
        OBJ="$ODIR/${REL%.c}.o"
        mkdir -p "$(dirname "$OBJ")"
        echo "  → $SRC → $OBJ"
        $CC $CFLAGS -c "$SRC" -o "$OBJ"
    done
}

compile_cc_files() {
    echo "Building all .cc files under $CDIR (recursive)..."
    find "$CDIR" -type f -name '*.cc' -print0 | while IFS= read -r -d '' SRC; do
        REL=${SRC#${CDIR}/}
        OBJ="$ODIR/${REL%.cc}.o"
        mkdir -p "$(dirname "$OBJ")"
        echo "  → $SRC → $OBJ"
        $CPP $CCFLAGS -c "$SRC" -o "$OBJ"
    done
}

compile_asm_files() {
    echo "Building all assembly (.S/.s) files under $CDIR (recursive)..."
    find "$CDIR" -type f \( -name '*.S' -o -name '*.s' \) -print0 | while IFS= read -r -d '' SRC; do
        REL=${SRC#${CDIR}/}
        OBJ="$ODIR/${REL%.*}.o"
        mkdir -p "$(dirname "$OBJ")"
        echo "  → $SRC → $OBJ"
        $CC $CFLAGS -c "$SRC" -o "$OBJ"
    done
}

compile_c_files
compile_cc_files
compile_asm_files

echo "Linking all .o files under $ODIR..."
# Find all .o files under $ODIR and pass them to the linker.
OBJ_FILES=$(find "$ODIR" -type f -name '*.o' -print)
$CPP $CCFLAGS -Wl,-T$CDIR/kernel.ld -Wl,-Map=$ODIR/kernel.map $OBJ_FILES -o $ODIR/kernel.elf

# Run QEMU
$QEMU -machine virt -bios default -nographic -serial mon:stdio --no-reboot -smp $QEMU_SMP -kernel $ODIR/kernel.elf