esp32-httpd-demo                                             A.Werner, 2018-05-23
---------------------------------------------------------------------------------

This is a httpd webserver based on the work from spritetm, chmorgen, MightyPork
and others.
' https://github.com/Spritetm/libesphttpd
* https://github.com/Spritetm/esphttpd
* https://github.com/chmorgan/libesphttpd
* https://github.com/MightyPork/esphttpd



Setup ESP-IDF under Windows Command Shell
-----------------------------------------

cd C:\Espressif
git clone https://github.com/espressif/esp-idf.git esp-idf

pushd C:\Espressif\esp-idf
git checkout 2aa9c15906b259dd188e88c144cb422eaa091b27    &: May 18, 2018 - good
git submodule update --init --checkout  --recursive
popd

pushd C:\Espressif\esp-idf
git checkout 18e83bcd53f2aacd667959af755f8a443fb2a57e    &: May 18, 2018 - bad
git submodule update --init --recursive --checkout
popd

cd /D F:\Projects\InternetOfThings\Devices\WebServer\Firmware\WebServer\source
C:\msys32\mingw32.exe


Build under MinGW Shell
-----------------------

export IDF_PATH=C:/Espressif/esp-idf
pushd F:/Projects/Evaluation/ESP-WROOM-32/Firmware/esp32-httpd-demo/source

clear;make -j 5  flash monitor
clear;make -j 9  flash monitor


code beautification
-------------------
# removed -o    Don't break lines containing multiple statements into multiple single-statement lines.
#         -O    Don't break blocks residing completely on one line.
#         -xj   Remove brackets from a bracketed one line conditional statements.

# set     --lineend=linux    OR  -z2

#"D:\Program Files (x86)\AStyle\bin\AStyle.exe" -A1 -s3 -c -D -S -p -U -z2    -R *.c *.h
"D:\Program Files (x86)\AStyle\bin\AStyle.exe"  -A1 -s3 -c -D -S -p -U -xW -xl -z2 -N -m2 -O -o -C -R *.c *.h

del /s .\*.orig .\*.bak


Setup WLAN Access Point on Windows Host
---------------------------------------
netsh wlan stop hostednetwork
netsh wlan set hostednetwork mode=allow ssid=esp-wlan key=1234567890 keyUsage=persistent
netsh wlan start hostednetwork


Core Dump
---------
ELF_FILE=../build//esp32-webserver-demo.elf
COREDUMP=../coredump.txt

$IDF_PATH/components/espcoredump/espcoredump.py info_corefile -t b64 -c $COREDUMP $ELF_FILE
$IDF_PATH/components/espcoredump/espcoredump.py dbg_corefile -t b64 -c $COREDUMP $ELF_FILE
