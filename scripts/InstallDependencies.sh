#!/bin/bash

INSTALLED_PACKAGE=false
PACKAGE_NAME="gcc"
dpkg -l $PACKAGE_NAME | grep -e "^ii" > intall_cache.txt
if [ -z "$(cat "intall_cache.txt")" ]; then
    echo "Installing $PACKAGE_NAME..."
    apt-get -qq -y install $PACKAGE_NAME >/dev/null 2>&1
    echo "$PACKAGE_NAME successfully installed!"
    INSTALLED_PACKAGE=true
else
    echo "$PACKAGE_NAME is already installed!"
fi

PACKAGE_NAME="libssl-dev"
dpkg -l $PACKAGE_NAME | grep -e "^ii" > intall_cache.txt
if [ -z "$(cat "intall_cache.txt")" ]; then
    echo "Installing $PACKAGE_NAME..."
    apt-get -qq -y install $PACKAGE_NAME >/dev/null 2>&1
    echo "$PACKAGE_NAME successfully installed!"
    INSTALLED_PACKAGE=true
else
    echo "$PACKAGE_NAME is already installed!"
fi

PACKAGE_NAME="libreadline-dev"
dpkg -l $PACKAGE_NAME | grep -e "^ii" > intall_cache.txt
if [ -z "$(cat "intall_cache.txt")" ]; then
    echo "Installing $PACKAGE_NAME..."
    apt-get -qq -y install $PACKAGE_NAME >/dev/null 2>&1
    echo "$PACKAGE_NAME successfully installed!"
    INSTALLED_PACKAGE=true
else
    echo "$PACKAGE_NAME is already installed!"
fi

PACKAGE_NAME="git"
dpkg -l $PACKAGE_NAME | grep -e "^ii" > intall_cache.txt
if [ -z "$(cat "intall_cache.txt")" ]; then
    echo "Installing $PACKAGE_NAME..."
    apt-get -qq -y install $PACKAGE_NAME >/dev/null 2>&1
    echo "$PACKAGE_NAME successfully installed!"
    INSTALLED_PACKAGE=true
else
    echo "$PACKAGE_NAME is already installed!"
fi

PACKAGE_NAME="openssl"
dpkg -l $PACKAGE_NAME | grep -e "^ii" > intall_cache.txt
if [ -z "$(cat "intall_cache.txt")" ]; then
    echo "Installing $PACKAGE_NAME..."
    apt-get -qq -y install $PACKAGE_NAME >/dev/null 2>&1
    echo "$PACKAGE_NAME successfully installed!"
    INSTALLED_PACKAGE=true
else
    echo "$PACKAGE_NAME is already installed!"
fi

if [ $INSTALLED_PACKAGE ]; then
    rm intall_cache.txt
fi
# PACKAGE_NAME="git cmake"
# if ! dpkg -s $PACKAGE_NAME >/dev/null 2>&1; then
#     echo "Installing $PACKAGE_NAME..."
#     apt-get -qq -y install $PACKAGE_NAME >/dev/null 2>&1
#     echo "$PACKAGE_NAME successfully installed!"
# fi

# if ! [ -d "lib/libiconv" ]; then
#     cd lib
#     wget http://ftp.gnu.org/pub/gnu/libiconv/libiconv-1.11.tar.gz
#     tar -xvzf libiconv-1.11.tar.gz
#     mv libiconv-1.11 libiconv
#     cd libiconv
#     ./configure
#     make
#     cd ..
#     rm libiconv-1.11.tar.gz
# fi
