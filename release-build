#!/bin/bash
if [ $# -eq 0 ] ; then
    echo "specify version as n.n.n"
	exit
fi
git checkout master
rm -rf release
mkdir release
cd release
cmake .. -DUSB_CONSOLE=OFF
make -j8
cp *.elf *.hex *.bin *.uf2 ..
rm -rf *
cmake .. -DUSB_CONSOLE=ON
make -j8
cp *.elf *.hex *.bin *.uf2 ..
cmake .. -DPICO_BOARD=vgaboard
make -j8
cp pshell_usb.elf ../vgaboard.pshell_usb.elf
cp pshell_usb.hex ../vgaboard.pshell_usb.hex
cp pshell_usb.bin ../vgaboard.pshell_usb.bin
cp pshell_usb.uf2 ../vgaboard.pshell_usb.uf2
cd ..
rm -rf release
tar czf pshell_$1.tgz *.elf *.hex *.bin *.uf2
rm *.elf *.hex *.bin *.uf2
