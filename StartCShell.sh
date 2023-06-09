#!/bin/bash
clear

CJSON_PATH=$(readlink -f "./lib/cJSON")
if ! [ -z $CJSON_PATH ]; then
    LD_LIBRARY_PATH_VAR="$CJSON_PATH:"

    if [[ $(echo $LD_LIBRARY_PATH) == $LD_LIBRARY_PATH_VAR""$LD_LIBRARY_PATH_VAR ]]; then
        ./CShell
    else
        echo Please execute this command and try again: 
        echo "export LD_LIBRARY_PATH=$LD_LIBRARY_PATH_VAR""$LD_LIBRARY_PATH_VAR"
    fi
else
    echo Cannot get path of the cJSON Folder, please reboot your system to make it work!
fi