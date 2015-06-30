The command to flash a new esp firmware is like this:

esptool.exe -p COM129 -b 115200 write_flash 0x00000 "eagle.flash.bin" 0x10000 "eagle.irom0text.bin"