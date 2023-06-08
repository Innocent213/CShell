rm config.json
rm server.crt
rm server.key
rm CShell
if [ -f "fifocom" ]; then
    unlink fifocom
fi
cd ./lib
rm CSHLib.o
rm libCSHLib.a
rm NSSHLib.o
rm libNSSHLib.a
# rm -r cJSON --force
cd ../Utils
rm NSSHClient