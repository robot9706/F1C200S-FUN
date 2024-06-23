# F1C200S FUN

This repo contains random projects and code for the **F1C200S** (and **F1C100S**) CPUs.

## Build

I wrote a buildtool which uses YAML to call various compilation tools. I don't know why this seemed like a good idea at 2AM.

## Run

### Using an SD card with UBoot

1. Copy the build binary to an SD card (FAT32) and insert it into whatever devkit you have.
2. Reset the CPU and spam enter to drop into a UBoot shell.
3. Execute `fatload mmc 0:1 80000000 fun.bin; go 80000000;`, where `fun.bin` is the name of the binary you want to run.

## Random notes

The docs does not seem to be correct about the memory layout. BROM is at 0xffff0000, while the SRAM is at 0x0.
From 0x0 - 0x8000 it seems to be kinda usable, but the 0x2000-0x4000 range sometimes has random garbage after wirting to it.
It seems the first 32k might be something else which looks like sram.

## Sources

Thanks to the following guides and repos for the work they've done:

- https://github.com/nminaylov/F1C100s_projects
- https://github.com/minilogic/f1c_nonos
- https://github.com/nminaylov/F1C100s_info
- https://linux-sunxi.org/USB_OTG_Controller_Register_Guide
- https://beyondlogic.org/usbnutshell/usb6.shtml
- https://www.cnblogs.com/yanye0xcc/p/16341719.html
