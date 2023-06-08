#!/bin/bash

gcc NSSHClient.c -o NSSHClient -L../lib/cJSON -L../lib -lcjson  -lNSSHLib -lcrypto -lssl -lreadline