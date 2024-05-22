# F1C200S FUN

This repo contains random projects and code for the **F1C200S** (and **F1C100S**) CPUs.

## Build

TODO

## Run

### Using an SD card with UBoot

1. Copy the build binary to an SD card (FAT32) and insert it into whatever devkit you have.
2. Reset the CPU and spam enter to drop into a UBoot shell.
3. Execute `fatload mmc 0:1 80000000 fun.bin; go 80000000;`, where `fun.bin` is the name of the binary you want to run.

## Sources

Thanks to the following guides and repos for the work they've done:

- https://github.com/nminaylov/F1C100s_projects
- https://github.com/minilogic/f1c_nonos
- https://github.com/nminaylov/F1C100s_info
- https://linux-sunxi.org/USB_OTG_Controller_Register_Guide
