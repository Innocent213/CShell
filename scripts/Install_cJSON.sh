#!/bin/bash

INSTALL_DIR="./lib/cJSON"
if ! [ -d "$INSTALL_DIR" ]; then
    cd lib
    git clone https://github.com/DaveGamble/cJSON.git
    cmake ..
    make
    echo "cJSON wurde erfolgreich installiert."
fi