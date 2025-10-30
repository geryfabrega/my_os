#!/bin/sh
qemu-system-x86_64 -cdrom  image.iso -boot d -m 512M -smp 1
