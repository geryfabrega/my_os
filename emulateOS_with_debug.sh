#!/bin/sh
qemu-system-x86_64 -cdrom  image.iso -boot d -m 512M -smp 1 -s -S
# the -s tells it to start a gdb server on tcp::1234m -S tells it to freeze on boot up
