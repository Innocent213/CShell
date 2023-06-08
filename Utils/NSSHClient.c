#include "../lib/NSSHLib.h"
#include "../lib/cJSON/cJSON.h"
#include <stdio.h>
#include <string.h>

int main() {
    char IP[40];
    char buffer[10000] = {0};
    int running = 1;

    printf("Gib die IP der CShell ein: ");
    fgets(IP, 40, stdin);
    IP[strlen(IP)-1] = '\0';

    NSSHClient *client = nsshClient_Initialize(IP, 8921);
    printf("Connecting to Server '%s' with the port '%d'...\n", inet_ntoa(client->client.sin_addr), client->port);
    if (nsshClient_Connect(client)) {
        cJSON *json_data = cJSON_CreateObject();
        cJSON_AddItemToObject(json_data, "hostname", cJSON_CreateString(client->hostname));
        cJSON_AddItemToObject(json_data, "mac", cJSON_CreateString(client->mac));
        nsshClient_SendMessage(client, cJSON_PrintUnformatted(json_data));
        cJSON_Delete(json_data);

        char message[10];
        nsshClient_ReciveMessage(client, message);

        if (strcmp(message, "DONE") == 0) {
            while(running) {
                printf("> ");
                fgets(buffer, 10000, stdin);
                buffer[strlen(buffer)-1] = '\0';
                
                if (strcmp(buffer, "exit") == 0) {
                    running = 0;
                } else if (strcmp(buffer, "clear") == 0) {
                    system("clear");
                } else {
                    // Befehl ausführen
                    cJSON *root = cJSON_CreateObject();
                    cJSON_AddItemToObject(root, "mode", cJSON_CreateString("execute"));
                    cJSON_AddItemToObject(root, "command", cJSON_CreateString(buffer));

                    char *json = cJSON_PrintUnformatted(root);
                    nsshClient_SendMessage(client, json);
                    cJSON_Delete(root);
                    
                    while(1) {
                        nsshClient_ReciveMessage(client, buffer);
                        root = cJSON_Parse(buffer);
                        char *mode = cJSON_GetStringValue(cJSON_GetObjectItem(root, "mode"));
                        if (strcmp(mode, "print") == 0) {
                            char *message = cJSON_GetStringValue(cJSON_GetObjectItem(root, "message"));
                            printf("%s", message);
                        } else if (strcmp(mode, "printJSON") == 0) {
                            printf("%s", cJSON_PrintUnformatted(cJSON_GetObjectItem(root, "json")));
                        } else if (strcmp(mode, "done") == 0) {
                            break;
                        }
                        cJSON_Delete(root);
                    }
                }
            }
        } else if (strcmp(message, "BANNED") == 0) {
            printf("Cannot connect to Server: You are banned!\n");
        } else {
            printf("Cannot connect to Server: Unknown Reason!\n");
        }
        nsshClient_Close(client);
    }
    return 0;
}