%2\tools\srec_cat.exe -o .\UVBuild\info_sbl.hex -I %2\soc\arm_cm\le501x\bin\bram.bin -Bin -of 0x18000300 -crc32-l-e -max-a %2\soc\arm_cm\le501x\bin\bram.bin -Bin -of 0x18000300 %2\tools\le501x\info.bin -Bin -of 0x18000000 -gen 0x1800001c 0x18000020 -const-l-e 0x18000300 4 -gen 0x18000020 0x18000024 -const-l-e -l %2\soc\arm_cm\le501x\bin\bram.bin -Bin 4 -gen 0x18000024 0x18000028 -const-l-e %3 4 -gen 0x1800002c 0x18000030 -const-l-e 0x1807c000 4 -gen 0x18000030 0x18000036 -rep-d 0xff 0xff 0xff 0xff 0xff 0xff
fromelf --bin --output=.\UVBuild\%1.ota.bin .\UVBuild\%1.axf
fromelf --i32 --output=.\UVBuild\%1.hex .\UVBuild\%1.axf

REM Add key verification to app bin file, then convert to hex
%2\tools\EDCH\bin\Debug\SIGN.exe %2\project\ble\LE501X_project\gen_key.bin .\UVBuild\%1.ota.bin
%2\tools\srec_cat.exe -o .\UVBuild\%1_signed.hex -I .\UVBuild\%1.ota.bin -Bin -of %3

if "%4" NEQ "" (
%2\tools\srec_cat.exe -Output .\UVBuild\%1_production.hex -Intel .\UVBuild\info_sbl.hex -Intel .\UVBuild\%1_signed.hex -Intel %2\%4 -Intel
) else (
%2\tools\srec_cat.exe -Output .\UVBuild\%1_production.hex -Intel .\UVBuild\info_sbl.hex -Intel .\UVBuild\%1_signed.hex -Intel
)
fromelf -c -a -d -e -v -o .\UVBuild\%1.asm .\UVBuild\%1.axf
