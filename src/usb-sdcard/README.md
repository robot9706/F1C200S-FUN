A USB SD card reader.

`YACC.exe --build build.yaml --build-arg PROJECTROOT=. --build-arg TOOLBIN=arm-gnu-toolchain-13.2.Rel1-mingw-w64-i686-arm-none-eabi\bin`

`fatload mmc 0:1 80000000 build.bin; go 80000000;`
