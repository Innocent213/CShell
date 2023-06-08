rm config.json --force
rm server.crt --force
rm server.key --force
rm CShell --force
if [ -f "fifocom" ]; then
    unlink fifocom
fi
cd ./lib
rm CSHLib.o --force
rm libCSHLib.a --force
rm NSSHLib.o --force
rm libNSSHLib.a --force
# rm -r cJSON --force
cd ../Utils
rm NSSHClient --force