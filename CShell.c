#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include <unistd.h>
#include <pthread.h>
#include <sys/ioctl.h>

#include <fcntl.h>
#include <libgen.h>

#include <dirent.h>
#include <sys/stat.h>
#include <utime.h>
#include <time.h>
#include <wait.h>

#include <readline/readline.h>
#include <readline/history.h>

#include "./lib/CSHLib.h"
#include "./lib/NSSHLib.h"
#include "./lib/cJSON/cJSON.h"

#define FIFOPATH "./fifocom"
#define BIN_FOLDER_PATH "./bin"
#define CONFIG_FILE_PATH "./config.json"
#define PIPE_READ 0
#define PIPE_WRITE 1

#define RCERROR_NONE 0
#define RCERROR_NOTFOUND 1

#define MAX_COMMAND 20
#define MAX_MODULE_NAME 20
#define MAX_MODULE_VERSION 10
#define MAX_MODULE_DESCRIPTION 200

#define MAX_ALIAS_STRING 20
#define MAX_ALIASES 10
#define MAX_ARGS 20
#define MAX_ARGS_LENGTH 10

typedef struct NSSHServerStruct NSSHServer;

typedef struct {
    char executable_path[MAX_PATH];
    char folder_path[MAX_PATH];
    int alias_count;
    CSHManpage *manpage;
    CSHModuleInfo Info;
    ModuleCommand commands[MAX_COMMANDS];
    ModuleEvent events[MAX_EVENTS];
    int command_count;
    int event_count;
    bool enabled;
} MODULE;

struct handle_client_thread_args{
    NSSHClient *client;
    NSSHServer *server;
};

typedef struct NSSHServerStruct {
    int server_socket;
    struct sockaddr_in server;
    int running; 

    SSL_CTX* ctx;
    SSL* ssl;

    int port;
    pthread_t threads[MAX_THREADS];
    struct handle_client_thread_args hcta[MAX_THREADS];
    int client_count;
} NSSHServer;

/*
ls
touch
*/

/*
Ideen:
Es gibt C Programme, die wie Module sind, die ausgeführt werden können.
Dafür gibt es eine eigene Bibliothek, die dafür Sorgt, dass man aus der Konsole Fehler ausgeben kann 
und es gibt verschiedene Funktionen wie preStart, start, finisch usw. Es gibt auch noch andere verschiedene EventHandler
Pipes(Zeichen: | ) werden auch noch implementiert und es wird auch csh Skripte geben
Helpbefehl zählt alle Befehle von dem Modulen auf und die Beschreibung

Ausführung:
    - benannte Pipes(FIFOs)
    - Pipes
    - Fork

Features:
    - Commandhistory
    - Module:
        - Events
        - Commands (Manpages, Aliases)
        - Disable/Enable
        - Remote Commands:
            - Shell Shutdown
            - PrintInfo / PrintError / PrintFatal / PrintNormal
        - Config

Bekannte Probleme:
    - Programm stürzt ab, wenn es zum ersten Mal gestartet wird und sich ein Client verbindet
    - Programm stürtz ab, wenn man den Befehl 'nssh clients' ausführt und man aber mit 'cd' den Ordner der CShell verlassen hat und nicht mehr zurückwechselt

Todo:
    - Autovervollständigung
    - SSL Verschlüsselung bei den PIPES (OPENSSL-Bibliothek)
    - Skripte
    - Modules:
        - getConfig & setConfig
    - Tunnels (Ähnlich wie Pipe: Befehl wird ausgeführt, aber nicht ausgegeben, sondern in ein Array aus Strings geschrieben und zurückgegeben)
    - Für Module eine Art GUI Libary implementieren, womit sie ihr eigenes TUI in der Konsole erstellen können (Buttons, Textfelder, Events)
    - Paketmanager für Module
*/

char *readCommand();
int executeCommand(NSSHClient *client, char *str, bool silent_mode);
int executeRemoteCommand(NSSHClient *client, int pipe, char *str, bool silent_mode);

int strSplit(char *str, char *delimiter, char **array);
void strTrim(char *str);

void printNormal(NSSHClient *client, char *message, bool silent_mode);
void printError(NSSHClient *client, char *message, bool silent_mode);
void printFatal(NSSHClient *client, char *message, bool silent_mode);
void printInfo(NSSHClient *client, char *message, bool silent_mode);
void printWarning(NSSHClient *client, char *message, bool silent_mode);
void print(NSSHClient *client, FILE *stdstream, const char *message, const char *prefix, char *color, bool silent_mode);
void printJSON(NSSHClient *client, cJSON *json, bool silent_mode);
void showProgress(int progress, int max);

char** getFilesInFolder(const char* folderPath, int* count);
void freeFileList(char** fileList, int count);
int isExecutable(const char *path);
int hasFileExtentsion(const char *path, const char *ext);
int fileExists(const char *path);
void rmSubstr(char* str, const char* substring);
void remove_directory(const char *path);
int directoryExists(const char *path);
int createFile(char *path);
int ModuleExists(char *name);

int GetModuleIDByCommand(char *cmd);
CSHManpage *GetManpageByCommand(char *cmd);
int GetModuleIDByName(char *name);

void InitModules();
void InitShell();
void ShutdownShell(int error_code);
void InitNetwork();

CSHConfig GetConfig();
void SetConfig(CSHConfig config);
int IsModuleDisabled(MODULE module);

NSSHServer *NSSHServer_Initialize(int port);
int NSSHServer_Start(NSSHServer *server);
int NSSHServer_Stop(NSSHServer *server);
int NSSHServer_SendMessage(NSSHClient *clientdata, char *message);
int NSSHServer_RecieveMessage(NSSHClient *clientdata, char *message);
int NSSHServer_DisconnectClient(NSSHClient *client, int thread_cancel);
int NSSHServer_GetClientID(NSSHClient *client);
int NSSHServer_IsClientBanned(char *mac);
int NSSHServer_IsClientBannedByName(char *name);
int NSSHServer_GetClientIDByName(char *name);

SSL_CTX* createSSLContext();

MODULE modules[MAX_MODULES];
int module_count = 0;

NSSHServer *server;

int main(int argc, char **argv) {
    InitShell();

    // Initialisiere die GNU Readline-Bibliothek
    rl_attempted_completion_function = NULL;
    rl_initialize();

    while(1) {
        char *buffer = readCommand();
        int error = executeCommand(NULL, buffer, false);
        if (error == 1) {
            char message[100];
            sprintf(message, "Command not found! Enter the Command %shelp%s to get a List of all Commands.\n", COLOR_CYAN, COLOR_RED);
            printError(NULL, message, 0);
        }
    }
}

char *readCommand() {
    char *buffer = readline("> ");
    if (strlen(buffer) != 0) {
        add_history(buffer);
    
        // Behandle das Drücken der Pfeiltaste nach oben
        if (buffer[0] == '\0' && history_length > 1) {
            // Hole den vorherigen Befehl aus der Historie
            HIST_ENTRY* prev_entry = previous_history();

            if (prev_entry != NULL) {
                // Kopiere den vorherigen Befehl in die Eingabezeile
                strncpy(buffer, prev_entry->line, BUFSIZ - 1);
                buffer[BUFSIZ - 1] = '\0';
            }
        }
    }
    return buffer;
}

int executeCommand(NSSHClient *client, char *str, bool silent_mode) {
    char *args[10];
    if (strlen(str) != 0) {
        int arg_count = strSplit(str, " ", args);
        if (strcmp(args[0], "touch") == 0) {
            char path[MAX_PATH];
            char dir[MAX_PATH];
            char full_path[MAX_PATH];
            strcpy(path, full_path);
            strcpy(dir, path);
            dirname(dir);
            if (!fileExists(path)) {
                FILE *file = fopen(args[1], "w+");
                fwrite("", sizeof(""), strlen(""), file);
                fclose(file);
            } else if (!directoryExists(dir)) {
                if (silent_mode) { printError(NULL, "Directory does not exist!", silent_mode); }
            } else {
                struct utimbuf times;
                times.actime = times.modtime = time(NULL);
                int error = utime(path, &times);
            }
        } else if (strcmp(args[0], "ls") == 0) {
            char buffer[1024];
            char command[10];
            if (arg_count == 2) {
                sprintf(command, "%s %s", args[0], args[1]);
            } else {
                strcpy(command, args[0]);
            }
            FILE* pipe = popen(command, "r");
            if (pipe == NULL) {
                printError(client, "Error while opening pipe to command 'ls'\n", silent_mode);
                return 1;
            }

            while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
                strcat(buffer, "\n");
                printNormal(client, buffer, silent_mode);
            }
            pclose(pipe);
        } else if (strcmp(args[0], "cat") == 0) {
            char path[MAX_PATH];
            realpath(args[1], path);
            if (fileExists(path)) {
                char buffer[100000];
                FILE *file = fopen(path, "r+");
                fread(buffer, sizeof(char), 100000, file);
                strcat(buffer, "\n");
                if (silent_mode) printNormal(NULL, buffer, silent_mode);
                fclose(file);
            }
        } else if (strcmp(args[0], "mkdir") == 0) {
            if (arg_count == 2) {
                char path[MAX_PATH];
                realpath(args[1], path);
                if (!directoryExists(path)) {
                    mkdir(path, 0700);
                }
            } else {
                printError(NULL, "To few Arguments!\n", silent_mode);
            }
        } else if (strcmp(args[0], "chmod") == 0) {
            if (arg_count == 3) {
                char path[MAX_PATH];
                int code = atoi(args[1]);
                realpath(args[2], path);
                if (fileExists(path)) {
                    if (chmod(path,code) < 0) printError(client, "Error while changing permissions!", silent_mode);
                }
            } else if (arg_count < 2) {
                if (silent_mode) printError(client, "Not enought Arguments!\n", silent_mode);
            }
        } else if (strcmp(args[0], "man") == 0) {
            if (arg_count == 2) {
                CSHManpage *manpage = GetManpageByCommand(args[1]);
                if (manpage != NULL) {
                    ManpageTitle *title = manpage->Title;
                    
                    // Print Title
                    struct winsize w;
                    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
                    int terminalWidth = w.ws_col;
                    int textLength = strlen(title->Text);

                    int leftPadding = (terminalWidth - textLength) / 2;
                    int rightPadding = terminalWidth - textLength - leftPadding;

                    for (int i = 0; i < leftPadding; i++) {
                       printNormal(client, "-", silent_mode);
                    }
                    char message[1000000];
                    sprintf(message, "%s%s%s%s%s%s%s", title->Color, title->Italic == 1 ? FORMAT_ITALIC : "", title->Underscore == 1 ? FORMAT_UNDERSCORE : "", title->Crossout == 1 ? FORMAT_CROSSOUT : "", title->Text, COLOR_RESET, FORMAT_RESET);
                    printNormal(client, message, silent_mode);
                    for (int i = 0; i < rightPadding; i++) {
                        printNormal(client, "-", silent_mode);
                    }
                    printNormal(client, "\n\n", silent_mode);
                    // Print Paragraphs
                    for (int i = 0; i < manpage->paragraph_count; i++) {
                        ManpageParagraph paragraph = manpage->Paragraphs[i];
                        char message[1000000];
                        sprintf(message, "%s%s%s%s%s%s%s\n", paragraph.Title->Color, paragraph.Title->Italic == 1 ? FORMAT_ITALIC : "", paragraph.Title->Underscore == 1 ? FORMAT_UNDERSCORE : "", paragraph.Title->Crossout == 1 ? FORMAT_CROSSOUT : "", paragraph.Title->Text, COLOR_RESET, FORMAT_RESET);
                        printNormal(client, message, silent_mode);
                        sprintf(message, "%s%s%s%s%s%s%s\n", paragraph.Content->Color, paragraph.Content->Italic == 1 ? FORMAT_ITALIC : "", paragraph.Content->Underscore == 1 ? FORMAT_UNDERSCORE : "", paragraph.Content->Crossout == 1 ? FORMAT_CROSSOUT : "", paragraph.Content->Text, COLOR_RESET, FORMAT_RESET);
                        printNormal(client, message, silent_mode);
                    }
                    printNormal(client, "\n", silent_mode);
                    for (int i = 0; i < terminalWidth; i++) {
                        printNormal(client, "-", silent_mode);
                    }
                    printNormal(client, "\n", silent_mode);
                } else {
                    printError(client, "No Manpage found!\n", silent_mode);
                }
            } else {
                printError(client, "Not enough Arguments!\n", silent_mode);
            }
        } else if (strcmp(args[0], "cd") == 0) {
            if (arg_count == 2) {
                if (!directoryExists(args[1])) {
                    printError(client, "This Directory does not exist!\n", silent_mode);
                } else if (chdir(args[1]) != 0) {
                    printError(client, "There was an error while changing directory!\n", silent_mode);
                }
            }
        } else if (strcmp(args[0], "rm") == 0) {
            if (arg_count == 2) {
                remove(args[1]);
            } else if (arg_count == 3) {
                if (strcmp(args[1], "-r") == 0) {
                    remove_directory(args[2]);
                }
            } else {
                printError(client, "Not the right amount of Arguments!\n", silent_mode);
            } 
        } else if (strcmp(args[0], "cwd") == 0) {
            char cwd[1024];
            getcwd(cwd, sizeof(cwd));
            char message[100000];
            sprintf(message, "Current Directory: %s%s%s\n", COLOR_BLUE, cwd, COLOR_RESET);
            printNormal(client, message, silent_mode);
        } else if (strcmp(args[0], "module") == 0) {
            if (arg_count == 2) {
                if (strcmp(args[1], "list") == 0) {
                    printNormal(client, "Modules: ", silent_mode);
                    for (int i = 0; i < module_count; i++) {
                        char message[MAX_MODULE_NAME+30];
                        sprintf(message, "%s%s%s", modules[i].enabled ? COLOR_GREEN : COLOR_RED, modules[i].Info.name, COLOR_RESET);
                        printNormal(client, message, silent_mode);
                        if (i != module_count-1) {
                            printNormal(client, ", ", silent_mode);
                        }
                    }
                    printNormal(client, "\n", silent_mode);
                } else if (strcmp(args[1], "reload") == 0) {
                    module_count = 0;
                    InitModules();
                }
            } else if (arg_count == 3) {
                int module_id = GetModuleIDByName(args[1]);
                if (module_id != -1) {
                    if (strcmp(args[2], "info") == 0) {
                        CSHModuleInfo Info = modules[module_id].Info;
                        char message[10000];
                        sprintf(message, "%s(%s) by %s: \n", Info.name, Info.version, Info.author);
                        printNormal(client, message, silent_mode);
                        sprintf(message, "%s\n", Info.description);
                        printNormal(client, message, silent_mode);
                    } else if (strcmp(args[2], "enable") == 0) {
                        int disabled = IsModuleDisabled(modules[module_id]);
                        if (disabled > -1) {
                            modules[module_id].enabled = true;
                            CSHConfig config = GetConfig();
                            for (int i = disabled; i < --config.disabled_modules_count; i++) {
                                strcpy(config.disabled_modules[i], config.disabled_modules[i+1]);
                            }
                            SetConfig(config);
                        } else {
                            printError(client, "This Module is already enabled!\n", silent_mode);
                        }
                    } else if (strcmp(args[2], "disable") == 0) {
                        int disabled = IsModuleDisabled(modules[module_id]);
                        if (disabled == -1) {
                            modules[module_id].enabled = false;
                            CSHConfig config = GetConfig();
                            strcpy(config.disabled_modules[config.disabled_modules_count], modules[module_id].Info.name);
                            config.disabled_modules_count++;
                            SetConfig(config);
                        } else {
                            printError(client, "This Module is already disabled!\n", silent_mode);
                        }
                    } else {
                        printError(client, "This Action does not exist!\n", silent_mode);
                    }
                } else{
                    printError(client, "This Module does not exist!\n", silent_mode);
                }
            }
        } else if (strcmp(args[0], "clear") == 0) {
            system("clear");
        } else if (strcmp(args[0], "libary") == 0) {
            CSHConfig config = GetConfig();
            if (arg_count == 4) {
                if (strcmp(args[1], "add") == 0) {
                    CSHConfig config = GetConfig();
                    CSHLibary libary;
                    strcpy(libary.folder_path, args[2]);
                    strcpy(libary.libary_name, args[3]);
                    config.libarys[config.libary_count] = libary;
                    config.libary_count++;
                    SetConfig(config);
                }
            } else if (arg_count == 3) {
                if (strcmp(args[1], "remove") == 0) {
                    int libary_id = -1;
                    for (int i = 0; i < config.libary_count; i++) {
                        if (strcmp(config.libarys[i].libary_name, args[2]) == 0) {
                            libary_id = i;
                        }
                    }

                    if (libary_id > -1) {
                        for (int i = libary_id; i < config.libary_count-1; i++) {
                            config.libarys[i] = config.libarys[i+1];
                        }
                        config.libary_count--;
                    } else {
                        printError(client, "This Libary wasn't found in the Database!", silent_mode);
                    }
                    SetConfig(config);
                }
            } else if (arg_count == 2) {
                if (strcmp(args[1], "list") == 0) {
                    printNormal(client, "Libaries: ", silent_mode);
                    for (int i = 0; i < config.libary_count; i++) {
                        printNormal(client, COLOR_BLUE, silent_mode);
                        printNormal(client, config.libarys[i].libary_name, silent_mode);
                        printNormal(client, COLOR_RESET, silent_mode);
                        if (i != config.libary_count-1) {
                            printNormal(client, ", ", silent_mode);
                        }
                    }
                    printNormal(client, "\n", silent_mode);    
                }
            }
        } else if (strcmp(args[0], "nssh") == 0) {
            CSHConfig config = GetConfig();
            if (strcmp(args[1], "port") == 0) {
                if (arg_count == 2) {
                    printNormal(client, "NSSH Port: ", silent_mode);
                    char port[10];
                    sprintf(port, "%d", config.nssh_config.port);
                    printNormal(client, COLOR_BLUE, silent_mode);
                    printNormal(client, port, silent_mode);
                    printNormal(client, COLOR_RESET, silent_mode);
                    printNormal(client, "\n", silent_mode);
                } else if (arg_count == 3) {
                    config.nssh_config.port = atoi(args[2]);
                    SetConfig(config);
                }
            } else if (strcmp(args[1], "enable") == 0) {
                if (!config.nssh_config.enabled) {
                    config.nssh_config.enabled = true;
                    SetConfig(config);
                    printNormal(client, "Restart the Shell to apply the change!\n", silent_mode);
                }
            } else if (strcmp(args[1], "disable") == 0) {
                if (config.nssh_config.enabled) {
                    config.nssh_config.enabled = false;
                    SetConfig(config);
                    printNormal(client, "Restart the Shell to apply the change!\n", silent_mode);
                }
            } else if (strcmp(args[1], "state") == 0) {
                printNormal(client, "State: ", silent_mode);
                printNormal(client, COLOR_BLUE, silent_mode);
                printNormal(client, config.nssh_config.enabled ? "Enabled" : "Disabled", silent_mode);
                printNormal(client, COLOR_RESET, silent_mode);
                printNormal(client, "\n", silent_mode);
            } else if (strcmp(args[1], "clients") == 0) {
                if (server->running) {
                    if (server->client_count != 0) {
                        printNormal(client, "Connected Clients: ", silent_mode);
                        for (int i = 0; i < server->client_count; i++) {
                            printNormal(client, COLOR_BLUE, silent_mode);
                            printNormal(client, server->hcta[i].client->hostname, silent_mode);
                            printNormal(client, COLOR_RESET, silent_mode);
                            if (i != server->client_count-1) {
                                printNormal(client, ", ", silent_mode);
                            }
                        }
                        printNormal(client, "\n", silent_mode);
                    } else {
                        printNormal(client, COLOR_BLUE, silent_mode);
                        printNormal(client, "There are no Clients connected!\n", silent_mode);
                        printNormal(client, COLOR_RESET, silent_mode);
                    }
                    CSHConfig config = GetConfig();
                    if (config.nssh_config.banned_clients_count > 0) {
                        printNormal(client, "Banned Clients: ", silent_mode);
                        for (int i = 0; i < config.nssh_config.banned_clients_count; i++) {
                            printNormal(client, COLOR_RED, silent_mode);
                            printNormal(client, config.nssh_config.banned_clients[i].name, silent_mode);
                            printNormal(client, COLOR_RESET, silent_mode);
                            if (i != config.nssh_config.banned_clients_count-1) {
                                printNormal(client, ", ", silent_mode);
                            }
                        }
                        printNormal(client, "\n", silent_mode);
                    } else {
                        printNormal(client, COLOR_RED, silent_mode);
                        printNormal(client, "There are no Clients banned!\n", silent_mode);
                        printNormal(client, COLOR_RESET, silent_mode);
                    }
                } else {
                    printError(client, "NSSH-Server is not running!", silent_mode);
                }
            } else if (arg_count == 4) {
                CSHConfig config = GetConfig();
                if (strcmp(args[3], "unban") == 0) {
                    int banned_client_id = NSSHServer_IsClientBannedByName(args[2]);
                    if (banned_client_id != -1) {
                        for (int i = banned_client_id; i < config.nssh_config.banned_clients_count-1; i++) {
                            config.nssh_config.banned_clients[i] = config.nssh_config.banned_clients[i+1];
                        }
                        config.nssh_config.banned_clients_count--;
                        SetConfig(config);
                    } else {
                        printError(client, "This Client is not banned!\n", silent_mode);
                    }
                } else if (strcmp(args[3], "ban") == 0) {
                    if (NSSHServer_IsClientBanned(args[2]) == -1) {
                        int client_id = NSSHServer_GetClientIDByName(args[2]);
                        if (client_id != -1) {
                            strcpy(config.nssh_config.banned_clients[config.nssh_config.banned_clients_count].name, server->hcta[client_id].client->hostname);
                            strcpy(config.nssh_config.banned_clients[config.nssh_config.banned_clients_count].mac, server->hcta[client_id].client->mac);
                            config.nssh_config.banned_clients_count++;
                            NSSHServer_DisconnectClient(server->hcta[client_id].client, true);
                        } else {
                            printError(client, "This Client is not connected!\n", silent_mode);
                        }
                    } else {
                        printError(client, "This Client is already banned!\n", silent_mode);
                    }
                    SetConfig(config);
                } else {
                    int client_id = NSSHServer_GetClientIDByName(args[2]);
                    if (client_id != -1) {
                        if (strcmp(args[3], "kick") == 0) {
                            NSSHServer_DisconnectClient(server->hcta[client_id].client, true);
                        }
                    } else {
                        printError(client, "This Client is not connected!\n", silent_mode);
                    }
                }
            }
        } else if (strcmp(args[0], "help") == 0) {
            printNormal(client, COLOR_GREEN, silent_mode);
            printNormal(client, "touch", silent_mode);
            printNormal(client, COLOR_RESET, silent_mode);
            printNormal(client, ": Creates a file and changes the access date of an already existing file.\n", silent_mode);
            printNormal(client, COLOR_GREEN, silent_mode);
            printNormal(client, "ls", silent_mode);
            printNormal(client, COLOR_RESET, silent_mode);
            printNormal(client, ": Lists all files in a folder.\n", silent_mode);
            printNormal(client, COLOR_GREEN, silent_mode);
            printNormal(client, "cat", silent_mode);
            printNormal(client, COLOR_RESET, silent_mode);
            printNormal(client, ": Outputs the content of a file.\n", silent_mode);
            printNormal(client, COLOR_GREEN, silent_mode);
            printNormal(client, "clear", silent_mode);
            printNormal(client, COLOR_RESET, silent_mode);
            printNormal(client, ": Clears the console.\n", silent_mode);
            printNormal(client, COLOR_GREEN, silent_mode);
            printNormal(client, "mkdir", silent_mode);
            printNormal(client, COLOR_RESET, silent_mode);
            printNormal(client, ": Creates a folder.\n", silent_mode); 
            printNormal(client, COLOR_GREEN, silent_mode);
            printNormal(client, "chmod", silent_mode);
            printNormal(client, COLOR_RESET, silent_mode);
            printNormal(client, ": Changes the permissions of a file or folder.\n", silent_mode);
            printNormal(client, COLOR_GREEN, silent_mode);
            printNormal(client, "man", silent_mode);
            printNormal(client, COLOR_RESET, silent_mode);
            printNormal(client, ": Displays a command's man page.\n", silent_mode);
            printNormal(client, COLOR_GREEN, silent_mode);
            printNormal(client, "cd", silent_mode);
            printNormal(client, COLOR_RESET, silent_mode);
            printNormal(client, ": Changes the shell's 'Current Working Directory'.\n", silent_mode);
            printNormal(client, COLOR_GREEN, silent_mode);
            printNormal(client, "cwd", silent_mode);
            printNormal(client, COLOR_RESET, silent_mode);
            printNormal(client, ": Displays from the shell's 'Current Working Directory'.\n", silent_mode);
            printNormal(client, COLOR_GREEN, silent_mode);
            printNormal(client, "module", silent_mode);
            printNormal(client, COLOR_RESET, silent_mode);
            printNormal(client, ": Manages the modules.\n", silent_mode);
            printNormal(client, COLOR_GREEN, silent_mode);
            printNormal(client, "help", silent_mode);
            printNormal(client, COLOR_RESET, silent_mode);
            printNormal(client, ": Displays a list of all available commands.\n", silent_mode);
            printNormal(client, COLOR_GREEN, silent_mode);
            printNormal(client, "rm", silent_mode);
            printNormal(client, COLOR_RESET, silent_mode);
            printNormal(client, ": Deletes a file | Recursively deletes a folder with the '-r' argument\n", silent_mode);
            printNormal(client, COLOR_GREEN, silent_mode);
            printNormal(client, "libary", silent_mode);
            printNormal(client, COLOR_RESET, silent_mode);
            printNormal(client, ": Manages the external libraries for the modules:\n", silent_mode);
            printNormal(client, COLOR_GREEN, silent_mode);
            printNormal(client, "libary", silent_mode);
            printNormal(client, COLOR_RESET, silent_mode);
            printNormal(client, ": Manages the external libraries for the modules:\n", silent_mode);
            printNormal(client, COLOR_GREEN, silent_mode);
            printNormal(client, "nssh", silent_mode);
            printNormal(client, COLOR_RESET, silent_mode);
            printNormal(client, ": Verlatet den NSSH-Server.\n", silent_mode);
            for (int i = 0; i < module_count; i++) {
                MODULE module = modules[i];
                for (int j = 0; j < module.command_count; j++) {
                    ModuleCommand commandStruct = module.commands[j];
                    printNormal(client, COLOR_BLUE, silent_mode);
                    printNormal(client, commandStruct.command, silent_mode);
                    printNormal(client, COLOR_RESET, silent_mode);
                    if (commandStruct.alias_count > 0) {
                        printNormal(client, "(", silent_mode);
                    }
                    for (int k = 0; k < commandStruct.alias_count; k++) {
                        printNormal(client, COLOR_MAGENTA, silent_mode);
                        printNormal(client, commandStruct.aliases[k], silent_mode);
                        printNormal(client, COLOR_RESET, silent_mode);
                        if (k != commandStruct.alias_count-1) {
                            printNormal(client, " | ", silent_mode);
                        }
                    }
                    if (commandStruct.alias_count > 0) {
                        printNormal(client, ")", silent_mode);
                    }
                    if (strlen(commandStruct.description) != 0) {
                        printNormal(client, ": ", silent_mode);
                    }
                    printNormal(client, commandStruct.description, silent_mode);
                    printNormal(client, "\n", silent_mode);
                }
            }
        } else if (strcmp(args[0], "exit") == 0) {
            if (arg_count == 1) {
                ShutdownShell(atoi(args[0]));
            } else {
                ShutdownShell(0);
            }
        } else {
            int module_id = GetModuleIDByCommand(args[0]);
            if (module_id == -1) {
                return 1;
            } else if (modules[module_id].enabled) {
                char fifoResolvedPath[MAX_PATH];
                realpath(FIFOPATH, fifoResolvedPath);
                
                cJSON *root = cJSON_CreateObject();
                cJSON *jsonArray = cJSON_CreateArray();

                cJSON_AddStringToObject(root, "command", args[0]);
                for (int i = 1; i < arg_count; i++) {
                    cJSON_AddItemToArray(jsonArray, cJSON_CreateString(args[i]));
                }
                cJSON_AddItemToObject(root, "args", jsonArray);
                cJSON_AddNumberToObject(root, "arg_count", arg_count-1);

                char *json_data = cJSON_PrintUnformatted(root);
                int pid = fork();
                if (pid == 0) {
                    execl(modules[module_id].executable_path, "exec", modules[module_id].folder_path, fifoResolvedPath, json_data, NULL);
                }

                int pipe = open(FIFOPATH, O_RDWR);
                char buffer[300];
                while(1) {
                    memset(buffer, 0, sizeof(buffer));
                    read(pipe, buffer, sizeof(buffer));
                    cJSON *root = cJSON_Parse(buffer);
                    cJSON *json_error = cJSON_GetObjectItem(root, "error");
                    int error = cJSON_GetNumberValue(json_error);
                    if (json_error != NULL) {
                        if (error == 0) {
                            break;
                        } else if (error == 1) {
                            char message[30+MAX_ERROR];
                            sprintf(message, "Failed to execute Command: %s\n", cJSON_GetStringValue(cJSON_GetObjectItem(root, "message")));
                            printError(client, message, silent_mode);
                            break;
                        }
                    } else {
                        int error = executeRemoteCommand(client, pipe, buffer, silent_mode);
                        if (error == RCERROR_NOTFOUND) {
                            printError(client, "Could not execute remote command!", silent_mode);
                        }
                    }
                }
                close(pipe);
                waitpid(pid, NULL, 0);
            }
        }
    }
    return 0;
}

void InitModules() {
    printInfo(NULL, "Initializing Modules...\n", 0);
    char **fileList;
    int count = 0;
    fileList = getFilesInFolder("Moduls", &count);
    int file_count = 0;
    for (int i = 0; i < count; i++) {
        if (hasFileExtentsion(fileList[i], ".c")) {
            file_count++;
        }
    }
    int all_modules_initialized = 1;
    for (int i = 0; i < count; i++) {
        if (hasFileExtentsion(fileList[i], ".c")) {
            char message[MAX_PATH+50];
            sprintf(message, "Initializing Module '%s'\n", realpath(fileList[i], NULL));
            printInfo(NULL, message, 0);
            
            // Zusammenstellen des Befehls zum kompilieren des Modules
            CSHConfig config = GetConfig();
            char command[200];
            strcpy(command, "gcc ");
            strcat(command, fileList[i]);
            strcat(command, " -o ");
            rmSubstr(fileList[i], ".c");
            strcat(command, fileList[i]);
            for (int i = 0; i < config.libary_count; i++) {
                strcat(command, " -L");
                strcat(command, config.libarys[i].folder_path);
                strcat(command, " -l");
                strcat(command, config.libarys[i].libary_name);
            }
            system(command);

            if (fileExists(fileList[i])) {
                // Inizialisiere Modul
                char fifoResolvedPath[MAX_PATH];
                char moduleResolvedPath[MAX_PATH];
                char moduleFolderPath[MAX_PATH];
                realpath(FIFOPATH, fifoResolvedPath);
                realpath(fileList[i], moduleResolvedPath);
                realpath(dirname(fileList[i]), moduleFolderPath);

                int pid = fork();
                if (pid == 0) {
                    execl(moduleResolvedPath, "init", moduleFolderPath, fifoResolvedPath, NULL);
                }

                int pipe;
                // Öffne die FIFO-Datei im Lese -und Schreibmodus
                pipe = open(FIFOPATH, O_RDWR);
                if (pipe == -1) {
                    printError(NULL, "Could not connect to Module via FIFO!", 0);
                    exit(EXIT_FAILURE);
                }

                char module_data[1000000];
                read(pipe, module_data, sizeof(module_data) - 1);
                // printNormal(NULL, module_data);

                cJSON *root = cJSON_Parse(module_data);
                MODULE module;


                // Module Info aus JSON herauslesen
                cJSON *json_module_info = cJSON_GetObjectItem(root, "info");
                strcpy(module.Info.name, cJSON_GetStringValue(cJSON_GetObjectItem(json_module_info, "name")));
                strcpy(module.Info.version, cJSON_GetStringValue(cJSON_GetObjectItem(json_module_info, "version")));
                strcpy(module.Info.description, cJSON_GetStringValue(cJSON_GetObjectItem(json_module_info, "description")));
                strcpy(module.Info.author, cJSON_GetStringValue(cJSON_GetObjectItem(json_module_info, "author")));

                cJSON *json_commands = cJSON_GetObjectItem(root, "commands");
                module.command_count = 0;
                for (int i = 0; i < cJSON_GetArraySize(json_commands); i++) {
                    cJSON *json_command = cJSON_GetArrayItem(json_commands, i);
                    ModuleCommand commandStruct;

                    strcpy(commandStruct.command, cJSON_GetStringValue(cJSON_GetObjectItem(json_command, "command")));
                    cJSON *json_aliases = cJSON_GetObjectItem(json_command, "aliases");
                    for (int i = 0; i < cJSON_GetArraySize(json_aliases); i++) {
                        strcpy(commandStruct.aliases[i], cJSON_GetStringValue(cJSON_GetArrayItem(json_aliases, i)));
                    }
                    strcpy(commandStruct.description, cJSON_GetStringValue(cJSON_GetObjectItem(json_command, "description")));
                    commandStruct.alias_count = cJSON_GetArraySize(json_aliases);
                    commandStruct.manpage = ManpageFromJSON(cJSON_GetObjectItem(json_command, "manpage"));

                    module.commands[module.command_count] = commandStruct;
                    module.command_count++;
                }
                
                cJSON *json_events = cJSON_GetObjectItem(root, "events");
                int event_count = cJSON_GetArraySize(json_events);
                for (int i = 0; i < event_count; i++) {
                    cJSON *json_event = cJSON_GetArrayItem(json_events, i);
                    char *event = cJSON_GetStringValue(cJSON_GetObjectItem(json_event, "event"));
                    strcpy(module.events[i].event, event);
                    module.event_count++;
                }
                
                char buffer[300];
                while(1) {
                    memset(buffer, 0, sizeof(buffer));
                    read(pipe, buffer, sizeof(buffer));
                    cJSON *root = cJSON_Parse(buffer);
                    cJSON *json_error = cJSON_GetObjectItem(root, "error");
                    int error = cJSON_GetNumberValue(json_error);
                    if (json_error != NULL) {
                        if (error == 0) {
                            if (IsModuleDisabled(module) == -1) {
                                module.enabled = true;
                            } else {
                                module.enabled = false;
                            }
                            strcpy(module.executable_path, moduleResolvedPath);
                            strcpy(module.folder_path, moduleFolderPath);
                            modules[module_count] = module;
                            module_count++;
                            break;
                        } else if (error == 1) {
                            all_modules_initialized = 0;
                            char message[30+MAX_ERROR];
                            sprintf(message, "Failed to initialize Module: %s\n", cJSON_GetStringValue(cJSON_GetObjectItem(root, "message")));
                            printError(NULL, message, 0);
                            break;
                        }
                    } else {
                        int error = executeRemoteCommand(NULL, pipe, buffer, 0);
                        if (error == RCERROR_NOTFOUND) {
                            printError(NULL, "Could not execute remote command!", 0);
                        }
                    }
                }
                waitpid(pid, NULL, 0);
                close(pipe);
            }
            // showProgress(i+1, file_count);
        }
    }
    if (all_modules_initialized) {
        printInfo(NULL, "All Modules were initialized successfully!\n", 0);
    } else {
        printWarning(NULL, "Some Moduls couldn't be initialized successfully!\n", 0);
    }
}

void InitConfig() {
    if (!fileExists(CONFIG_FILE_PATH)) {
        printInfo(NULL, "Creating Config File ...\n", 0);
        createFile(CONFIG_FILE_PATH);

        CSHConfig config;
        config.disabled_modules_count = 0;
        config.libary_count = 0;

        config.nssh_config.enabled = true;
        config.nssh_config.port = 8921;

        CSHLibary libary;
        strcpy(libary.folder_path, "./lib/cJSON");
        strcpy(libary.libary_name, "cjson");
        config.libarys[config.libary_count] = libary;
        config.libary_count++;

        strcpy(libary.folder_path, "./lib/");
        strcpy(libary.libary_name, "CSHLib");
        config.libarys[config.libary_count] = libary;
        config.libary_count++;

        SetConfig(config);
    }
}

void InitNetwork() {
    CSHConfig config = GetConfig();
    if (config.nssh_config.enabled) {
        server = NSSHServer_Initialize(config.nssh_config.port);
        NSSHServer_Start(server);
        if (!fileExists("server.crt") || !fileExists("server.crt")) {
            printInfo(NULL, "Creating SSL NSSHServer-Certificate ...\n", 0);
            
            createFile("openssl.cnf");
            char config[1000] = { "[req]\nprompt = no\ndistinguished_name = req_distinguished_name\n\n[req_distinguished_name]\nC = US\nST = State\nL = City\nO = Organization\nOU = Organizational Unit\nCN = Common Name\n" };
            FILE *file = fopen("openssl.cnf", "w+");
            fwrite(config, sizeof(config), strlen(config), file);
            fclose(file);
            
            system("openssl req -x509 -newkey rsa:4096 -nodes -keyout server.key -out server.crt -days 365 -config openssl.cnf");
            remove("openssl.cnf");
        }
    }
}

void InitShell() {
    printInfo(NULL, "Initializing Shell...\n", 0);
    
    // Erstelle die FIFO-Datei
    if (access(FIFOPATH, F_OK) != 0) {
        printInfo(NULL, "Initializing FIFO...\n", 0);
        int result = mkfifo(FIFOPATH, 0666);
        if (result == -1) {
            printError(NULL, "FIFO File could not be created!\n", 0);
            exit(EXIT_FAILURE);
        }
    } else {
        printInfo(NULL, "FIFO File does already exist! Skipping...\n", 0);
    }

    InitConfig();
    InitNetwork();
    InitModules();
    
    printInfo(NULL, "Shell initialized successfully!\n", 0);
}

void ShutdownShell(int error_code) {
    NSSHServer_Stop(server);
    if (server != NULL) {
        free(server);
        server = NULL;
    }
    unlink(FIFOPATH);
    exit(error_code);
}

int executeRemoteCommand(NSSHClient *client, int pipe, char *str, bool silent_mode) {
    char fifoResolvedPath[MAX_PATH];
    realpath(FIFOPATH, fifoResolvedPath);
    cJSON *root = cJSON_Parse(str);
    char *mode = cJSON_GetStringValue(cJSON_GetObjectItem(root, "mode"));
    if (strcmp(mode, "event") == 0) {
        char *event = cJSON_GetStringValue(cJSON_GetObjectItem(root, "event"));
        cJSON *json_data = cJSON_GetObjectItem(root, "data");
        for (int i = 0; i < module_count; i++) {
            MODULE module = modules[i];
            for (int j = 0; j < module.event_count; j++) {
                if (strcmp(module.events[j].event, event) == 0) {
                    int pid = fork();
                    if (pid == 0) {
                        execl(modules[i].executable_path, "event", modules[i].folder_path, fifoResolvedPath, str, NULL);
                    }

                    char buffer[300];
                    while(1) {
                        memset(buffer, 0, sizeof(buffer));
                        read(pipe, buffer, sizeof(buffer));
                        cJSON *root = cJSON_Parse(buffer);
                        cJSON *json_error = cJSON_GetObjectItem(root, "error");
                        int error = cJSON_GetNumberValue(json_error);
                        if (json_error != NULL) {
                            if (error == 0) {
                                break;
                            } else if (error == 1) {
                                char message[30+MAX_ERROR];
                                sprintf(message, "Failed to execute Module: %s\n", cJSON_GetStringValue(cJSON_GetObjectItem(root, "message")));
                                printError(client, message, silent_mode);
                                break;
                            }
                        } else {
                            int error = executeRemoteCommand(client, pipe, buffer, silent_mode);
                            if (error == RCERROR_NOTFOUND) {
                                printError(client, "Could not execute remote command!", silent_mode);
                            }
                        }
                    }
                }
            }
        }
    } else if (strcmp(mode, "printI") == 0) {
        cJSON *json_message = cJSON_GetObjectItem(root, "message");
        printInfo(client, cJSON_GetStringValue(json_message), silent_mode);
    } else if (strcmp(mode, "printE") == 0) {
        cJSON *json_message = cJSON_GetObjectItem(root, "message");
        printError(client, cJSON_GetStringValue(json_message), silent_mode);
    } else if (strcmp(mode, "printF") == 0) {
        cJSON *json_message = cJSON_GetObjectItem(root, "message");
        printFatal(client, cJSON_GetStringValue(json_message), silent_mode);
    } else if (strcmp(mode, "printN") == 0) {
        cJSON *json_message = cJSON_GetObjectItem(root, "message");
        printNormal(client, cJSON_GetStringValue(json_message), silent_mode);
    } else if (strcmp(mode, "printJSON") == 0) {
        cJSON *json = cJSON_GetObjectItem(root, "json");
        printJSON(client, json, silent_mode);
    } else if (strcmp(mode, "shutdown") == 0) {
        cJSON *json_error_code = cJSON_GetObjectItem(root, "error_code");
        ShutdownShell(cJSON_GetNumberValue(json_error_code));
    } else if (strcmp(mode, "execute") == 0) {
        char *args[MAX_ARGS];
        int arg_count = 0;
        char *command = cJSON_GetStringValue(cJSON_GetObjectItem(root, "command"));
        bool silent_mode = cJSON_GetObjectItem(root, "silent_mode")->valueint;
        int error = executeCommand(client, command, silent_mode);
        
        cJSON *json_reply = cJSON_CreateObject();
        cJSON_AddNumberToObject(json_reply, "error", error);
        char *json = cJSON_PrintUnformatted(json_reply);
        write(pipe, json, strlen(json));
    } else if (strcmp(mode, "getConfig") == 0) {
        cJSON *json_config = CSHConfigtoJSON(GetConfig());
        char *json = cJSON_PrintUnformatted(json_config);
        write(pipe, json, strlen(json));
        cJSON_Delete(json_config);
    } else {
        printf("\n");
        return 1;
    }
    cJSON_Delete(root);
    return 0;
}



int directoryExists(const char *path) {
    DIR *directory = opendir(path);
    if (directory) {
        closedir(directory);
        return 1;  // Das Verzeichnis existiert
    } else {
        return 0;  // Das Verzeichnis existiert nicht
    }
}

int isExecutable(const char *path) {
    struct stat st;
    if (stat(path, &st) == -1) {
        perror("Fehler bei stat");
        return 0;
    }
    return (st.st_mode & S_IXUSR) != 0;
}

int hasFileExtentsion(const char *path, const char *ext) {
    char *fileExtension = strrchr(path, '.');
    if (fileExtension != NULL && strcmp(fileExtension, ext) == 0) {
        return 1;
    } else {
        return 0;
    }

}

char** getFilesInFolder(const char* folderPath, int* count) {
    DIR* dir;
    struct dirent* entry;
    char** fileList = NULL;
    int fileCount = 0;

    // Öffne den Ordner
    dir = opendir(folderPath);
    if (dir == NULL) {
        char str[100];
        sprintf(str, "Error while opening Folder '%s'!\n", folderPath);
        printError(NULL, str, 0);
        exit(1);
    }

    // Zähle die Anzahl der Dateien im Ordner
    while ((entry = readdir(dir)) != NULL) {
        // Ignoriere "." und ".." (aktuelles Verzeichnis und Elternverzeichnis)
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // Erstelle den vollen Pfad zur Datei
        char filePath[258];  // Passe die Größe des Puffer-Arrays an deine Bedürfnisse an
        snprintf(filePath, sizeof(filePath), "%s/%s", folderPath, entry->d_name);

        // Überprüfe, ob es sich um eine reguläre Datei handelt
        FILE* file = fopen(filePath, "r");
        if (file != NULL) {
            fclose(file);
            fileCount++;
        }
    }

    // Setze den Dateizähler
    *count = fileCount;

    // Weise Speicher für das Dateiarray zu
    fileList = (char**)malloc(fileCount * sizeof(char*));
    if (fileList == NULL) {
        printError(NULL, "Fehler beim Speicherzuweisung.\n", 0);
        closedir(dir);
        return NULL;
    }

    // Setze den Dateinamen für jede gefundene Datei
    rewinddir(dir);
    int index = 0;
    while ((entry = readdir(dir)) != NULL) {
        // Ignoriere "." und ".." (aktuelles Verzeichnis und Elternverzeichnis)
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // Erstelle den vollen Pfad zur Datei
        char filePath[512];  // Passe die Größe des Puffer-Arrays an deine Bedürfnisse an
        snprintf(filePath, sizeof(filePath), "%s/%s", folderPath, entry->d_name);

        // Überprüfe, ob es sich um eine reguläre Datei handelt
        FILE* file = fopen(filePath, "r");
        if (file != NULL) {
            fclose(file);
            // Weise Speicher für den Dateipfad zu
            fileList[index] = (char*)malloc((strlen(filePath) + 1) * sizeof(char));
            if (fileList[index] == NULL) {
                printError(NULL, "Fehler beim Speicherzuweisung.\n", 0);
                closedir(dir);
                freeFileList(fileList, fileCount);  // Speicher freigeben
                return NULL;
            }

            // Kopiere den Dateipfad in das Array
            strcpy(fileList[index], filePath);
            index++;
        }
    }

    // Schließe den Ordner
    closedir(dir);

    return fileList;
}

void freeFileList(char** fileList, int count) {
    if (fileList == NULL) {
        return;
    }

    for (int i = 0; i < count; i++) {
        if (fileList[i] != NULL) {
            free(fileList[i]);
        }
    }

    free(fileList);
}

void remove_directory(const char *path) {
    DIR *dir = opendir(path);
    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL) {
        char file_path[MAX_PATH+MAX_PATH];
        struct stat file_stat;

        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        snprintf(file_path, sizeof(file_path), "%s/%s", path, entry->d_name);

        if (lstat(file_path, &file_stat) == -1) {
            perror("lstat");
            continue;
        }

        if (S_ISDIR(file_stat.st_mode)) {
            remove_directory(file_path);
        } else {
            if (unlink(file_path) == -1) {
                perror("unlink");
                continue;
            }
        }
    }

    closedir(dir);

    if (rmdir(path) == -1) {
        perror("rmdir");
    }
}



int GetModuleIDByCommand(char *cmd) {
    int module_id = -1;
    for (int i = 0; i < module_count; i++) {
        MODULE module = modules[i];
        if (module.enabled) {
            for (int j = 0; j < module.command_count; j++) {
                ModuleCommand commandStruct = module.commands[j];
                if (strcmp(commandStruct.command, cmd) == 0) {
                    module_id = i;
                }
                for (int k = 0; k < commandStruct.alias_count; k++) {
                    if (strcmp(commandStruct.aliases[k], cmd) == 0) {
                        module_id = i;
                    }
                }
            }
        }
    }
    return module_id;
}

CSHManpage *GetManpageByCommand(char *cmd) {
    for (int i = 0; i < module_count; i++) {
        MODULE module = modules[i];
        for (int j = 0; j < module.command_count; j++) {
            ModuleCommand commandStruct = module.commands[j];
            if (strcmp(commandStruct.command, cmd) == 0) {
                return commandStruct.manpage;
            }
            for (int k = 0; k < commandStruct.alias_count; k++) {
                if (strcmp(commandStruct.aliases[k], cmd) == 0) {
                    return commandStruct.manpage;
                }
            }
        }
    }
    return NULL;
}

int GetModuleIDByName(char *name) {
    int module_id = -1;
    for (int i = 0; i < module_count; i++) {
        if (strcmp(modules[i].Info.name, name) == 0) {
            module_id = i;
        }
    }
    return module_id;
}

int ModuleExists(char *name) {
    int module_exists = 0;
    for (int i = 0; i < module_count; i++) {
        if (modules[i].Info.name) {
            module_exists = 1;
            break;
        }
    }
    return module_exists;
}

int strSplit(char *str, char *delimiter, char **array) {
    char *token = strtok(str, delimiter);
    int index = 0;
    while (token != NULL) {
        array[index++] = token;
        token = strtok(NULL, delimiter);
    }
    return index;
}

void strTrim(char *str) {
    str[strlen(str)-1] = '\0';
}

void rmSubstr(char* str, const char* substring) {
    size_t len = strlen(substring);
    size_t strLen = strlen(str);

    if (strLen >= len) {
        char* endOfString = str + strLen - len;
        if (strcmp(endOfString, substring) == 0) {
            strncpy(endOfString, "", len);
        }
    }
}



void printNormal(NSSHClient *client, char *message, bool silent_mode) {
    print(client, stderr, message, NULL, COLOR_RESET, silent_mode);
}

void printError(NSSHClient *client, char *message, bool silent_mode) {
    print(client, stderr, message, "ERROR", COLOR_RED, silent_mode);
}

void printFatal(NSSHClient *client, char *message, bool silent_mode) {
    print(client, stderr, message, "FATAL", COLOR_RED, silent_mode);
}

void printInfo(NSSHClient *client, char *message, bool silent_mode) {
    print(client, stdout, message, "INFO", COLOR_WHITE, silent_mode);
}

void printWarning(NSSHClient *client, char *message, bool silent_mode) {
    print(client, stdout, message, "WARNING", COLOR_YELLOW, silent_mode);
}

void print(NSSHClient *client, FILE *stdstream, const char *message, const char *prefix, char *color, bool silent_mode) {    
    char lmessage[10000];
    if (prefix == NULL) {
        strcpy(lmessage, message);
    } else {
        time_t currentTime;
        struct tm* timeInfo;
        char timeString[80];

        // Aktuelle Zeit abrufen
        currentTime = time(NULL);
        // Zeitinformationen in eine strukturierte Form umwandeln
        timeInfo = localtime(&currentTime);
        // Zeitstempel-Formatierung festlegen
        strftime(timeString, sizeof(timeString), "%H:%M:%S", timeInfo);
        sprintf(lmessage, "%s[%s][%s] --> %s%s", color, timeString, prefix, message, COLOR_RESET);
    }
    if (lmessage[strlen(lmessage)-2] == '\n') {
        lmessage[strlen(lmessage)-1] = '\0';
    }
    if (client == NULL) {
        if (!silent_mode) printf("%s", lmessage);
    } else {
        cJSON *root = cJSON_CreateObject();
        cJSON_AddItemToObject(root, "mode", cJSON_CreateString("print"));
        cJSON_AddItemToObject(root, "message", cJSON_CreateString(lmessage));
        if (!silent_mode) NSSHServer_SendMessage(client, cJSON_PrintUnformatted(root));
        cJSON_Delete(root);
        sleep(0.1);
    }
}

void printJSON(NSSHClient *client, cJSON *json, bool silent_mode) {
    if (client == NULL) {
        if (!silent_mode) printf("%s", cJSON_PrintUnformatted(json));
    } else {
        cJSON *root = cJSON_CreateObject();
        cJSON_AddItemToObject(root, "mode", cJSON_CreateString("printJSON"));
        cJSON_AddItemToObject(root, "json", json);
        if (!silent_mode) NSSHServer_SendMessage(client, cJSON_PrintUnformatted(root));
        sleep(0.1);
    }
}

void showProgress(int progress, int max) {
    float percent = (float) progress/max;
    int fill_count = max*percent;
    printNormal(NULL, "\r", 0);
    for (int i = 0; i < max; i++) {
        if (i < fill_count) {
            printNormal(NULL, "#", 0);
        } else {
            printNormal(NULL, "-", 0);
        }
    }
    char message[30];
    sprintf(message, " [Progress: %.2f%%]", percent*100);
    printNormal(NULL, message, 0);
}



void SetConfig(CSHConfig config) {
    char *json = cJSON_Print(CSHConfigtoJSON(config));

    FILE *file = fopen(CONFIG_FILE_PATH, "w+");
    fprintf(file, "%s", json);
    fclose(file);
}

CSHConfig GetConfig() {
    char buffer[10000];
    FILE *file = fopen(CONFIG_FILE_PATH, "r+");
    fread(buffer, 10000, sizeof(buffer), file);
    fclose(file);

    cJSON *root = cJSON_Parse(buffer);
    CSHConfig config = CSHConfigfromJSON(root);
    cJSON_Delete(root);
    
    return config;
}

int IsModuleDisabled(MODULE module) {
    CSHConfig config = GetConfig();
    int disabled = -1;
    for (int i = 0; i < config.disabled_modules_count; i++) {
        if (strcmp(module.Info.name, config.disabled_modules[i]) == 0) {
            disabled = i;
            break;
        }
    }
    return disabled;
}



void *handle_client(struct handle_client_thread_args *hcta)
{
    while(1) {
        char message[10000];
        NSSHServer_RecieveMessage(hcta->client, message);
        cJSON *root = cJSON_Parse(message);
        char *mode = cJSON_GetStringValue(cJSON_GetObjectItem(root, "mode"));
        char *command = cJSON_GetStringValue(cJSON_GetObjectItem(root, "command"));
        if (strcmp(mode, "execute") == 0) {
            int error = executeCommand(hcta->client, command, false);
            if (error == 1) {
                char message[100];
                sprintf(message, "Command not found! Enter the Command %shelp%s to get a List of all Commands.\n", COLOR_CYAN, COLOR_RED);
                printError(hcta->client, message, 0);
            }
        }
        cJSON_Delete(root);
        
        root = cJSON_CreateObject();
        cJSON_AddItemToObject(root, "mode", cJSON_CreateString("done"));
        char *json = cJSON_PrintUnformatted(root);
        NSSHServer_SendMessage(hcta->client, json);
        cJSON_Delete(root);
    }
    close(hcta->client->socket);
}

int handle_connections(NSSHServer *server) {
    while(server->running)
    {
        // Accept connection from an incoming client
        struct sockaddr_in client;
        int client_socket = 0;

        // Accept and incoming connection
        // printNormal(NULL, "Waiting for incoming connections on port '%d'...\n", ntohs(server->server.sin_port));
        int c = sizeof(struct sockaddr_in);

        client_socket = accept(server->server_socket, (struct sockaddr *)&client, (int*)&c);
        if (client_socket < 0)
        {
            perror("accept failed with error code");
            return 1;
        }

        NSSHClient *clientdata = malloc(sizeof(NSSHClient));
        memcpy(&clientdata->client, &client, sizeof(struct sockaddr_in));
        clientdata->socket = client_socket;
        clientdata->ssl = SSL_new(server->ctx);
        SSL_set_fd(clientdata->ssl, client_socket);
        SSL_accept(clientdata->ssl);

        char recieved_data[1000];
        NSSHServer_RecieveMessage(clientdata, recieved_data);
        cJSON *json_data = cJSON_Parse(recieved_data);
        strcpy(clientdata->hostname, cJSON_GetStringValue(cJSON_GetObjectItem(json_data, "hostname")));
        strcpy(clientdata->mac, cJSON_GetStringValue(cJSON_GetObjectItem(json_data, "mac")));
        cJSON_Delete(json_data);
        if (NSSHServer_IsClientBanned(clientdata->mac) == -1) {
            if (server->client_count == MAX_THREADS) {
                // printNormal(NULL, "All threads are busy, unable to handle new connection\n");
                NSSHServer_SendMessage(clientdata, "CLOSE");
                close(client_socket);
            } else {
                // Create a new thread to handle the client
                server->hcta[server->client_count].client = clientdata;
                server->hcta[server->client_count].server = server;
                if(pthread_create(&server->threads[server->client_count] , NULL , (void *)handle_client, (struct handle_client_thread_args*)&server->hcta[server->client_count]) < 0)
                {
                    perror("could not create thread\n");
                    return 2;
                }

                // Detach the thread so that it can be terminated independently
                pthread_detach(server->threads[server->client_count]);
                server->client_count++;
                NSSHServer_SendMessage(clientdata, "DONE");
            }
        } else {
            NSSHServer_SendMessage(clientdata, "BANNED");
            close(client_socket);
        }
    }
}

int NSSHServer_SendMessage(NSSHClient *data, char *message) {
    int error = WriteMessage(data, message);
    if (error <= 0) {
        NSSHServer_DisconnectClient(data, false);
    }
    sleep(0.1);
    return error;
}

int NSSHServer_RecieveMessage(NSSHClient *data, char *message) {
    int error = ReadMessage(data, message);
    if (error <= 0) {
        NSSHServer_DisconnectClient(data, false);
    }
    return error;
}

int NSSHServer_Start(NSSHServer *server) {
    printInfo(NULL, "Starting NSSHServer...\n", 0);
    // Create a socket
    server->server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server->server_socket < 0) {
        printError(NULL, "Error while creating Socket!\n", 0);
        return -1;
    }

    // Bind the socket
    if (bind(server->server_socket, (struct sockaddr *)&server->server, sizeof(server->server)))
    {
        printError(NULL, "Bind failed!\n", 0);
        return -2;
    }

    // Listen for incoming connections
    listen(server->server_socket, 10);
    server->running = 1;
    long unsigned int thread_id;
    if(pthread_create( &thread_id, NULL, (void *) handle_connections, (void *)server) < 0)
    {
        printError(NULL, "could not create thread\n", 0);
    }
    pthread_detach(thread_id);
    printInfo(NULL, "NSSHServer successfully started...\n", 0);
}

NSSHServer *NSSHServer_Initialize(int port) {
    NSSHServer *server = malloc(sizeof(NSSHServer));
    if (server == NULL) {
        return NULL;
    }
    server->client_count = 0;
    server->server.sin_family = AF_INET;
    server->server.sin_addr.s_addr = INADDR_ANY;
    server->server.sin_port = htons(port);

    server->ctx = SSL_CTX_new(TLS_server_method());
    SSL_CTX_use_certificate_file(server->ctx, "server.crt", SSL_FILETYPE_PEM);
    SSL_CTX_use_PrivateKey_file(server->ctx, "server.key", SSL_FILETYPE_PEM);
    SSL_CTX_set_verify(server->ctx, SSL_VERIFY_NONE, NULL);
    return server;
}

int NSSHServer_Stop(NSSHServer *server) {
    if (server != NULL) {
        close(server->server_socket);
        server->running = 0;
    }
}

int NSSHServer_DisconnectClient(NSSHClient *client, int thread_cancel) {
    int client_id = NSSHServer_GetClientID(client);
    if (client_id != -1) {
        close(client->socket);
        SSL_shutdown(client->ssl);
        SSL_free(client->ssl);

        for (int i = client_id; i < server->client_count-1; i++) {
            server->hcta[i] = server->hcta[i+1];
            server->threads[i] = server->threads[i+1];
        }
        server->client_count--;
        if (thread_cancel) {
            pthread_cancel(server->threads[client_id]); // Kann zu unvorhersehbaren Verhalten führen, das muss aber so sein, sonst hält das Programm an
        } else {
            pthread_exit(&server->threads[client_id]);
        }
        free(client);
        return 1;
    }
    return 0;
}

int NSSHServer_IsClientBanned(char *mac) {
    CSHConfig config = GetConfig();
    for (int i = 0; config.nssh_config.banned_clients_count; i++) {
        if (strcmp(config.nssh_config.banned_clients[i].mac, mac) == 0) {
            return i;
        }
    }
    return -1;
}

int NSSHServer_IsClientBannedByName(char *name) {
    CSHConfig config = GetConfig();
    for (int i = 0; config.nssh_config.banned_clients_count; i++) {
        if (strcmp(config.nssh_config.banned_clients[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

int NSSHServer_GetClientID(NSSHClient *client) {
    for (int i = 0; i < server->client_count; i++) {
        if (server->hcta[i].client->socket = client->socket) {
            return i;
        }
    }
    return -1;
}

int NSSHServer_GetClientIDByName(char *name) {
    for (int i = 0; i < server->client_count; i++) {
        if (strcmp(server->hcta[i].client->hostname, name) == 0) {
            return i;
        }
    }
    return -1;
}