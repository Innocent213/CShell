PACKAGE_NAME="gcc"
if ! dpkg -s $PACKAGE_NAME >/dev/null 2>&1; then
    echo "Installing $PACKAGE_NAME..."
    apt-get -qq -y install $PACKAGE_NAME >/dev/null 2>&1
    echo "$PACKAGE_NAME successfully installed!"
fi

PACKAGE_NAME="libssl-dev"
if ! dpkg -s $PACKAGE_NAME >/dev/null 2>&1; then
    echo "Installing $PACKAGE_NAME..."
    apt-get -qq -y install $PACKAGE_NAME >/dev/null 2>&1
    echo "$PACKAGE_NAME successfully installed!"
fi

PACKAGE_NAME="libreadline-dev"
if ! dpkg -s $PACKAGE_NAME >/dev/null 2>&1; then
    echo "Installing $PACKAGE_NAME..."
    apt-get -qq -y install $PACKAGE_NAME >/dev/null 2>&1
    echo "$PACKAGE_NAME successfully installed!"
fi

PACKAGE_NAME="git"
if ! dpkg -s $PACKAGE_NAME >/dev/null 2>&1; then
    echo "Installing $PACKAGE_NAME..."
    apt-get -qq -y install $PACKAGE_NAME >/dev/null 2>&1
    echo "$PACKAGE_NAME successfully installed!"
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