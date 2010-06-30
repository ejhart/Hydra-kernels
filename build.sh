make clean
make ARCH=arm CROSS_COMPILE=arm-none-eabi- incrediblec_defconfig
make EXTRAVERSION=-cc1c2268 ARCH=arm CROSS_COMPILE=arm-none-eabi- zImage modules
cd ZipBuilder
./builder.sh
