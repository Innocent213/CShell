#!/bin/bash
clear

CJSON_PATH=$(readlink -f "../lib/cJSON")
LD_LIBRARY_PATH_VAR="$CJSON_PATH:"

if [[ $(echo $LD_LIBRARY_PATH) == $LD_LIBRARY_PATH_VAR""$LD_LIBRARY_PATH_VAR ]]; then
    ./NSSHClient
else
    echo Führe diesen Befehl aus und versuche es anschliesend erneut: 
    echo "export LD_LIBRARY_PATH=$LD_LIBRARY_PATH_VAR""$LD_LIBRARY_PATH_VAR"
fi
