#!/bin/bash

INSTALL_DIR="./lib/cJSON"
if ! [ -d "$INSTALL_DIR" ]; then
    cd lib
    git clone https://github.com/DaveGamble/cJSON.git
    cd cJSON
    cmake ..
    make
    echo "cJSON wurde erfolgreich installiert."
fi