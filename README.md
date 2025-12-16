# Userspace Scheduler
An in-progress userspace scheduler

## Current Features (will probably change soon)
- Preemptive Multithreading
- Semaphores, Mutexes, Promises, Reusable Barriers
- Shared Pointers

## Install Instructions on Linux

Install the following via your package manager's commands (below I use Debian/Ubuntu as an example)
```
sudo apt install qemu-system-riscv32
sudo apt install riscv64-unknown-elf-gcc
sudo apt install riscv64-unknown-elf-g++
sudo apt install mtools dosfstools
```

In the root directory, download OpenSBI
```
curl -LO https://github.com/qemu/qemu/raw/v8.0.4/pc-bios/opensbi-riscv32-generic-fw_dynamic.bin
```
