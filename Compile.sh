#!/bin/bash
clear

chmod 777 Cleanup.sh
chmod 777 Utils/CompileUtils.sh
chmod 777 scripts/InstallDependencies.sh
chmod 777 scripts/Install_cJSON.sh
bash scripts/InstallDependencies.sh
# bash scripts/Install_cJSON.sh

gcc -c lib/CSHLib.c -o lib/CSHLib.o -I../lib/cJSON -L../lib/cJSON -lcjson
ar rcs lib/libCSHLib.a lib/CSHLib.o

gcc -c lib/NSSHLib.c -o lib/NSSHLib.o -I../lib/cJSON -L../lib/cJSON -lcjson -lcrypto -lssl
ar rcs lib/libNSSHLib.a lib/NSSHLib.o

gcc CShell.c -o CShell -I../lib/cJSON -L../lib/cJSON -L./lib/ -lNSSHLib -lcjson -lCSHLib -lreadline -lcrypto -lssl
cd Utils
./CompileUtils.sh