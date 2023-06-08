#include "CSHLib.h"
#include "../lib/cJSON/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>

#define MAX_ARGS 20
#define MAX_ARGS_LENGTH 10
#define PIPE_WRITE 1
#define PIPE_READ 0

#define STATE_NONE 0
#define STATE_INIT 1
#define STATE_EXEC 2
#define STATE_EVENT 3

CSHModule *module = NULL;
int state = STATE_NONE;
char **global_args = NULL;
int global_argc = -1;

void fifo_send(char *str);



int createFile(char *path) {
    FILE *file = fopen(path, "w+");
    fwrite("{}", sizeof(char), strlen("{}"), file);
    fclose(file);
    return 0;
}

int fileExists(const char *filePath) {
    return access(filePath, F_OK) == 0;
}

void csh_InitModule(char **args, int argc) {
    module = malloc(sizeof(CSHModule));
    module->command_count = 0;
    module->event_count = 0;
    if (strcmp(args[0], "init") == 0) {
        state = STATE_INIT;
    } else if (strcmp(args[0], "exec") == 0) {
        state = STATE_EXEC;
    } else if (strcmp(args[0], "event") == 0) {
        state = STATE_EVENT;
    }
    global_args = args;
    global_argc = argc;
}

void csh_BuildModule(char *name, char *author, char *version, char *description, OnInitFunc onInit) {
    if (global_argc >= 3) {
        strcpy(module->Info.name, name);
        strcpy(module->Info.author, author);
        strcpy(module->Info.version, version);
        strcpy(module->Info.description, description);

        // Öffne die FIFO-Datei im Lesemodus
        module->pipe = open(global_args[2], O_RDWR);
        if (module->pipe == -1) {
            perror("Fehler beim Öffnen der FIFO-Datei (Lese -und Schreibemodus)");
            exit(EXIT_FAILURE);
        }

        module->onInit = onInit;
        module->folder_path = global_args[1];
        char *error;
        if (state == STATE_INIT) {
            // JSON-Objekt erstellen
            cJSON *root = cJSON_CreateObject();
            if (root == NULL) {
                printf("Error while creating JSON-object.\n");
                exit(EXIT_FAILURE);
            }

            // JSON-Werte hinzufügen
            cJSON *json_module_info = cJSON_CreateObject();
            cJSON_AddStringToObject(json_module_info, "name", module->Info.name);
            cJSON_AddStringToObject(json_module_info, "version", module->Info.version);
            cJSON_AddStringToObject(json_module_info, "description", module->Info.description);
            cJSON_AddStringToObject(json_module_info, "author", module->Info.author);

            // JSON-Werte für die Commands hinzufügen
            cJSON *json_commands = cJSON_CreateArray();
            for (int i = 0; i < module->command_count; i++) {
                ModuleCommand command = module->commands[i];
                cJSON *json_command = cJSON_CreateObject();
                cJSON *json_aliases = cJSON_CreateArray();
                cJSON_AddItemToObject(json_command, "command", cJSON_CreateString(command.command));
                cJSON_AddItemToObject(json_command, "description", cJSON_CreateString(command.description));
                for (int i = 0; i < command.alias_count; i++) {
                    cJSON_AddItemToArray(json_aliases, cJSON_CreateString(command.aliases[i]));
                }
                cJSON_AddItemToObject(json_command, "aliases", json_aliases);

                cJSON *json_manpage = ManpageToJSON(command.manpage);

                cJSON_AddItemToObject(json_command, "manpage", json_manpage);
                cJSON_AddItemToArray(json_commands, json_command);
            }

            cJSON *json_events = cJSON_CreateArray();
            for (int i = 0; i < module->event_count; i++) {
                cJSON *json_event = cJSON_CreateObject();
                cJSON_AddItemToObject(json_event, "event", cJSON_CreateString(module->events[i].event));
                cJSON_AddItemToArray(json_events, json_event);
            }

            cJSON_AddItemToObject(root, "commands", json_commands);
            cJSON_AddItemToObject(root, "events", json_events);
            cJSON_AddItemToObject(root, "info", json_module_info);

            char *json_data = cJSON_PrintUnformatted(root);
            fifo_send(json_data);

            // JSON-Objekt freigeben
            cJSON_Delete(root);
            error = module->onInit();
        } else if (state == STATE_EXEC) {
            char **exec_args = (char **)malloc(sizeof(char *)*MAX_ARGS);
            cJSON *root = cJSON_Parse(global_args[3]);
            cJSON *jsonArray = cJSON_GetObjectItem(root, "args");
            char *cmd = cJSON_GetStringValue(cJSON_GetObjectItem(root, "command"));
            for (int i = 0; i < cJSON_GetArraySize(jsonArray); i++) {
                exec_args[i] = strdup(cJSON_GetStringValue(cJSON_GetArrayItem(jsonArray, i)));
            }
            for (int i = 0; i < module->command_count; i++) {
                ModuleCommand commandStruct = module->commands[i];
                if (strcmp(commandStruct.command, cmd) == 0) {
                    error = commandStruct.onExecution(cmd, exec_args, cJSON_GetArraySize(jsonArray));
                    i = module->command_count;
                }
                for (int j = 0; j < commandStruct.alias_count; j++) {
                    if (strcmp(commandStruct.aliases[j], cmd) == 0) {
                        error = commandStruct.onExecution(cmd, exec_args, cJSON_GetArraySize(jsonArray));
                        i = module->command_count;
                    }
                }
            }
            for (int i = 0; i < cJSON_GetArraySize(jsonArray); i++) {
                free(exec_args[i]);
            }
            cJSON_Delete(root);
        } else if (state == STATE_EVENT) {
            cJSON *root = cJSON_Parse(global_args[3]);
            char *event = cJSON_GetStringValue(cJSON_GetObjectItem(root, "event"));
            cJSON *json_event_data = cJSON_GetObjectItem(root, "data");
            for (int i = 0; i < module->event_count; i++) {
                if (strcmp(module->events[i].event, event) == 0) {
                    error = module->events[i].onEvent(json_event_data);
                }
            }
            cJSON_Delete(root);
        }
        cJSON *root = cJSON_CreateObject();
        if (error == NULL) {
            cJSON_AddNumberToObject(root, "error", 0);
        } else {
            cJSON_AddNumberToObject(root, "error", 1);
            cJSON_AddItemToObject(root, "message", cJSON_CreateString(error));
        }
        char *json_data = cJSON_PrintUnformatted(root);
        // printf("%s\n", json_data);

        fifo_send(json_data);
        close(module->pipe);
    } else {
        fprintf(stderr, "To few Argumens\n");
    }
}

void csh_RegisterCommand(char *command, char **aliases, int alias_count, char *description, CSHManpage *manpage, OnExecutionFunc onExecution) {
    ModuleCommand commandStruct;
    strcpy(commandStruct.command, command);
    strcpy(commandStruct.description, description);
    commandStruct.manpage = manpage;
    for (int i = 0; i < alias_count; i++) {
        strcpy(commandStruct.aliases[i], aliases[i]);
    }
    commandStruct.alias_count = alias_count;
    commandStruct.onExecution = onExecution;
    module->commands[module->command_count] = commandStruct;
    module->command_count++;
}

void csh_RegisterEvent(char *event, OnEventFunc onEvent) {
    ModuleEvent eventStruct;
    strcpy(eventStruct.event, event);
    eventStruct.onEvent = onEvent;
    module->events[module->event_count] = eventStruct;
    module->event_count++;
}



void csh_showProgress(int progress, int max) {
    // char str[100];
    // char progress_str[10];
    // char max_str[10];
    // sprintf(progress_str, "%d", progress);
    // sprintf(max_str, "%d", max);
    // strcpy(str, "sp ");
    // strcat(str, progress_str);
    // strcat(str, " ");
    // strcat(str, max_str);
    // fifo_send(str);
}

CSHModule *csh_GetModule() {
    return module;
}

char *csh_GetWorkingDirectory() {
    return module->folder_path;
}


CSHManpage *Manpage_Init() {
    CSHManpage *manpage = malloc(sizeof(CSHManpage));
    manpage->paragraph_count = 0;
    manpage->Paragraphs = malloc(sizeof(ManpageParagraph));
    manpage->Title = malloc(sizeof(ManpageTitle));
    return manpage;
}

ManpageParagraph *Manpage_CreateParagraph() {
    return malloc(sizeof(ManpageParagraph));
}

ManpageTitle *Manpage_CreateTitle(char *titleStr) {
    ManpageTitle *title = malloc(sizeof(ManpageTitle));
    if (title != NULL) {
        strcpy(title->Text, titleStr);
    }
    return title;
}

ManpageContent *Manpage_CreateContent(char *contentStr) {
    ManpageContent *content = malloc(sizeof(ManpageTitle));
    if (content != NULL) {
        strcpy(content->Text, contentStr);
    }
    return content;
}

void Manpage_SetTitle(CSHManpage *manpage, ManpageTitle *title) {
    manpage->Title = title;
}

void Manpage_AddParagraph(CSHManpage *manpage, ManpageParagraph *paragraph) {
    manpage->Paragraphs[manpage->paragraph_count] = *paragraph;
    manpage->paragraph_count++;
}

void Manpage_SetParagraphContent(ManpageParagraph *paragraph, ManpageContent *content) {
    paragraph->Content = content;
}

void Manpage_SetParagraphTitle(ManpageParagraph *paragraph, ManpageTitle *title) {
    paragraph->Title = title;
}

void Manpage_SetTitleFormat(ManpageTitle *Title, char *color, int bold, int italic, int crossout, int underscore) {
    strcpy(Title->Color, color);
    Title->Bold = bold;
    Title->Italic = italic;
    Title->Crossout = crossout;
    Title->Underscore = underscore;
}

void Manpage_SetContentFormat(ManpageContent *Content, char *color, int bold, int italic, int crossout, int underscore) {
    strcpy(Content->Color, color);
    Content->Bold = bold;
    Content->Italic = italic;
    Content->Crossout = crossout;
    Content->Underscore = underscore;
}

void Manpage_Release(CSHManpage *manpage) {
    if (manpage != NULL) {
        free(manpage->Paragraphs);
        free(manpage->Title);
        free(manpage);
    }
}



void csh_printNormal(const char *format, ...) {
    char message[10000];
    va_list args;
    va_start(args, format);
    int length = vsnprintf(message, sizeof(message), format, args);
    va_end(args);


    cJSON *root = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "mode", cJSON_CreateString("printN"));
    cJSON_AddItemToObject(root, "message", cJSON_CreateString(message));
    char *json_data = cJSON_PrintUnformatted(root);
    fifo_send(json_data);
}

void csh_printInfo(const char *format, ...) {
    char message[10000];
    va_list args;
    va_start(args, format);
    int length = vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    cJSON *root = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "mode", cJSON_CreateString("printI"));
    cJSON_AddItemToObject(root, "message", cJSON_CreateString(message));
    char *json_data = cJSON_PrintUnformatted(root);
    fifo_send(json_data);
}

void csh_printError(const char *format, ...) {
    char message[10000];
    va_list args;
    va_start(args, format);
    int length = vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    cJSON *root = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "mode", cJSON_CreateString("printE"));
    cJSON_AddItemToObject(root, "message", cJSON_CreateString(message));
    char *json_data = cJSON_PrintUnformatted(root);
    fifo_send(json_data);
}

void csh_printFatal(const char *format, ...) {
    char message[10000];
    va_list args;
    va_start(args, format);
    int length = vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    cJSON *root = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "mode", cJSON_CreateString("printF"));
    cJSON_AddItemToObject(root, "message", cJSON_CreateString(message));
    char *json_data = cJSON_PrintUnformatted(root);
    fifo_send(json_data);
}

void csh_printJSON(cJSON *json) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "mode", cJSON_CreateString("printJSON"));
    cJSON_AddItemToObject(root, "json", json);
    char *json_data = cJSON_PrintUnformatted(root);
    fifo_send(json_data);
}



CSHManpage *ManpageFromJSON(cJSON *json) {
    if (json != NULL) {
        CSHManpage *manpage = malloc(sizeof(CSHManpage));
        manpage->paragraph_count = 0;
        manpage->Paragraphs = malloc(sizeof(ManpageParagraph));
        manpage->Title = malloc(sizeof(ManpageTitle));
        ManpageTitle *title = malloc(sizeof(ManpageTitle));
        
        cJSON *json_title = cJSON_GetObjectItem(json, "title");
        strcpy(title->Color, cJSON_GetStringValue(cJSON_GetObjectItem(json_title, "color")));
        strcpy(title->Text, cJSON_GetStringValue(cJSON_GetObjectItem(json_title, "text")));
        title->Bold = cJSON_GetNumberValue(cJSON_GetObjectItem(json_title, "bold"));
        title->Italic = cJSON_GetNumberValue(cJSON_GetObjectItem(json_title, "italic"));
        title->Crossout = cJSON_GetNumberValue(cJSON_GetObjectItem(json_title, "crossout"));
        title->Underscore = cJSON_GetNumberValue(cJSON_GetObjectItem(json_title, "underscore"));
        

        cJSON *json_paragraphs = cJSON_GetObjectItem(json, "paragraphs");
        for (int i = 0; i < cJSON_GetArraySize(json_paragraphs); i++) {
            cJSON *json_paragraph = cJSON_GetArrayItem(json_paragraphs, i);
            ManpageParagraph *paragraph = malloc(sizeof(ManpageParagraph));
            ManpageTitle *paragraph_title = malloc(sizeof(ManpageTitle));
            ManpageContent *paragraph_content = malloc(sizeof(ManpageContent));

            cJSON *json_title = cJSON_GetObjectItem(json_paragraph, "title");
            strcpy(paragraph_title->Color, cJSON_GetStringValue(cJSON_GetObjectItem(json_title, "color")));
            strcpy(paragraph_title->Text, cJSON_GetStringValue(cJSON_GetObjectItem(json_title, "text")));
            paragraph_title->Bold = cJSON_GetNumberValue(cJSON_GetObjectItem(json_title, "bold"));
            paragraph_title->Italic = cJSON_GetNumberValue(cJSON_GetObjectItem(json_title, "italic"));
            paragraph_title->Crossout = cJSON_GetNumberValue(cJSON_GetObjectItem(json_title, "crossout"));
            paragraph_title->Underscore = cJSON_GetNumberValue(cJSON_GetObjectItem(json_title, "underscore"));
            
            cJSON *json_content = cJSON_GetObjectItem(json_paragraph, "content");
            strcpy(paragraph_content->Color, cJSON_GetStringValue(cJSON_GetObjectItem(json_content, "color")));
            strcpy(paragraph_content->Text, cJSON_GetStringValue(cJSON_GetObjectItem(json_content, "text")));
            paragraph_content->Bold = cJSON_GetNumberValue(cJSON_GetObjectItem(json_content, "bold"));
            paragraph_title->Italic = cJSON_GetNumberValue(cJSON_GetObjectItem(json_content, "italic"));
            paragraph_title->Crossout = cJSON_GetNumberValue(cJSON_GetObjectItem(json_content, "crossout"));
            paragraph_title->Underscore = cJSON_GetNumberValue(cJSON_GetObjectItem(json_content, "underscore"));

            Manpage_SetParagraphTitle(paragraph, paragraph_title);
            Manpage_SetParagraphContent(paragraph, paragraph_content);
            Manpage_AddParagraph(manpage, paragraph);
        }
        Manpage_SetTitle(manpage, title);
        return manpage;
    }
    return NULL;
}

cJSON *ManpageToJSON(CSHManpage *manpage) {
    if (manpage != NULL) {
        cJSON *json_manpage = cJSON_CreateObject();

        if (manpage->Title != NULL) {
            cJSON *json_title = cJSON_CreateObject();
            cJSON_AddItemToObject(json_title, "color", cJSON_CreateString(manpage->Title->Color));
            cJSON_AddItemToObject(json_title, "text", cJSON_CreateString(manpage->Title->Text));
            cJSON_AddNumberToObject(json_title, "bold", manpage->Title->Bold);
            cJSON_AddNumberToObject(json_title, "italic", manpage->Title->Italic);
            cJSON_AddNumberToObject(json_title, "crossout", manpage->Title->Crossout);
            cJSON_AddNumberToObject(json_title, "underscore", manpage->Title->Underscore);
            cJSON_AddItemToObject(json_manpage, "title", json_title);
        }

        cJSON *json_paragraphs = cJSON_CreateArray();
        for (int i = 0; i < manpage->paragraph_count; i++) {
            cJSON *json_paragraph = cJSON_CreateObject();
            ManpageParagraph paragraph = manpage->Paragraphs[i];
            if (paragraph.Title != NULL) {
                cJSON *json_title = cJSON_CreateObject();
                cJSON_AddItemToObject(json_title, "color", cJSON_CreateString(paragraph.Title->Color));
                cJSON_AddItemToObject(json_title, "text", cJSON_CreateString(paragraph.Title->Text));
                cJSON_AddNumberToObject(json_title, "bold", paragraph.Title->Bold);
                cJSON_AddNumberToObject(json_title, "italic", paragraph.Title->Italic);
                cJSON_AddNumberToObject(json_title, "crossout", paragraph.Title->Crossout);
                cJSON_AddNumberToObject(json_title, "underscore", paragraph.Title->Underscore);
                cJSON_AddItemToObject(json_paragraph, "title", json_title);
            }
            if (paragraph.Content != NULL) {
                cJSON *json_content = cJSON_CreateObject();
                cJSON_AddItemToObject(json_content, "text", cJSON_CreateString(paragraph.Content->Text));
                cJSON_AddItemToObject(json_content, "color", cJSON_CreateString(paragraph.Content->Color));
                cJSON_AddNumberToObject(json_content, "bold", paragraph.Content->Bold);
                cJSON_AddNumberToObject(json_content, "italic", paragraph.Title->Italic);
                cJSON_AddNumberToObject(json_content, "crossout", paragraph.Title->Crossout);
                cJSON_AddNumberToObject(json_content, "underscore", paragraph.Title->Underscore);
                cJSON_AddItemToObject(json_paragraph, "content", json_content);
            }
            cJSON_AddItemToArray(json_paragraphs, json_paragraph);
        }
        cJSON_AddItemToObject(json_manpage, "paragraphs", json_paragraphs);
        return json_manpage;
    }
    return NULL;
}

CSHConfig CSHConfigfromJSON(cJSON *json) {
    CSHConfig config;
    cJSON *json_nsshConfig = cJSON_GetObjectItem(json, "nssh_config");
    config.libary_count = 0;
    config.nssh_config.enabled = cJSON_GetObjectItem(json_nsshConfig, "enabled")->valueint;
    config.nssh_config.port = cJSON_GetNumberValue(cJSON_GetObjectItem(json_nsshConfig, "port"));

    // Ausgeschaltete Module Liste laden
    cJSON *json_bannedClients = cJSON_GetObjectItem(json_nsshConfig, "banned_clients");
    config.nssh_config.banned_clients_count = 0;
    int bannedClientsCount = cJSON_GetArraySize(json_bannedClients);
    for (int i = 0; i < bannedClientsCount; i++) {
        cJSON *json_bannedClient = cJSON_GetArrayItem(json_bannedClients, i);
        strcpy(config.nssh_config.banned_clients[i].name, cJSON_GetStringValue(cJSON_GetObjectItem(json_bannedClient, "name")));
        strcpy(config.nssh_config.banned_clients[i].mac, cJSON_GetStringValue(cJSON_GetObjectItem(json_bannedClient, "mac")));
        config.nssh_config.banned_clients_count++;
    }

    // Ausgesch altete Module Liste laden
    cJSON *json_disabledModules = cJSON_GetObjectItem(json, "disabled_modules");
    config.disabled_modules_count = 0;
    int disabledModulesCount = cJSON_GetArraySize(json_disabledModules);
    for (int i = 0; i < disabledModulesCount; i++) {
        strcpy(config.disabled_modules[i], cJSON_GetStringValue(cJSON_GetArrayItem(json_disabledModules, i)));
        config.disabled_modules_count++;
    }

    // Bibliotheken laden
    cJSON *json_Libaries = cJSON_GetObjectItem(json, "libaries");
    int json_LibariesSize = cJSON_GetArraySize(json_Libaries);
    for (int i = 0; i < json_LibariesSize; i++) {
        cJSON *json_Libary = cJSON_GetArrayItem(json_Libaries, i);
        strcpy(config.libarys[i].folder_path, cJSON_GetStringValue(cJSON_GetObjectItem(json_Libary, "folder_path")));
        strcpy(config.libarys[i].libary_name, cJSON_GetStringValue(cJSON_GetObjectItem(json_Libary, "libary_name")));
        config.libary_count++;
    }

    return config;
}

cJSON *CSHConfigtoJSON(CSHConfig config) {
    cJSON *root = cJSON_CreateObject();
    cJSON *json_nsshConfig = cJSON_CreateObject();

    cJSON_AddBoolToObject(json_nsshConfig, "enabled", config.nssh_config.enabled);
    cJSON_AddNumberToObject(json_nsshConfig, "port", config.nssh_config.port);

    // Konvertiere gebannte Clients Liste
    cJSON *json_bannedClients = cJSON_CreateArray();
    for (int i = 0; i < config.nssh_config.banned_clients_count; i++) {
        cJSON *json_bannedClient = cJSON_CreateObject();
        cJSON_AddItemToObject(json_bannedClient, "name", cJSON_CreateString(config.nssh_config.banned_clients[i].name));
        cJSON_AddItemToObject(json_bannedClient, "mac", cJSON_CreateString(config.nssh_config.banned_clients[i].mac));
        cJSON_AddItemToArray(json_bannedClients, json_bannedClient);
    }
    cJSON_AddItemToObject(json_nsshConfig, "banned_clients", json_bannedClients);
        
    // Konvertiere ausgeschaltete Module Liste
    cJSON *json_disabledModules = cJSON_CreateArray();
    for (int i = 0; i < config.disabled_modules_count; i++) {
        cJSON_AddItemToArray(json_disabledModules, cJSON_CreateString(config.disabled_modules[i]));
    }

    // Konvertiere Bibliothek Liste
    cJSON *json_Libaries = cJSON_CreateArray();
    for (int i = 0; i < config.libary_count; i++) {
        cJSON *json_Libary = cJSON_CreateObject();
        CSHLibary libary = config.libarys[i];
        cJSON_AddItemToObject(json_Libary, "folder_path", cJSON_CreateString(libary.folder_path));
        cJSON_AddItemToObject(json_Libary, "libary_name", cJSON_CreateString(libary.libary_name));
        cJSON_AddItemToArray(json_Libaries, json_Libary);
    }

    cJSON_AddItemToObject(root, "libaries", json_Libaries);
    cJSON_AddItemToObject(root, "disabled_modules", json_disabledModules);
    cJSON_AddItemToObject(root, "nssh_config", json_nsshConfig);
    return root;
}



void csh_callEvent(char *event, cJSON *data) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "mode", cJSON_CreateString("event"));
    cJSON_AddItemToObject(root, "event", cJSON_CreateString(event));
    cJSON_AddItemToObject(root, "data", data);
    char *json = cJSON_PrintUnformatted(root);
    fifo_send(json);
    cJSON_Delete(root);
}

void csh_Shutdown(int error_code) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "mode", cJSON_CreateString("shutdown"));
    cJSON_AddNumberToObject(root, "error_code", error_code);
    char *json = cJSON_PrintUnformatted(root);
    fifo_send(json);
    cJSON_Delete(root);
}

int csh_ExecuteCommand(char *str, bool silent_mode) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "mode", cJSON_CreateString("execute"));
    cJSON_AddItemToObject(root, "command", cJSON_CreateString(str));
    cJSON_AddBoolToObject(root, "silent_mode", silent_mode);
    char *json = cJSON_PrintUnformatted(root);
    fifo_send(json);

    char buffer[1000];
    read(module->pipe, buffer, sizeof(buffer));
    cJSON *json_reply = cJSON_Parse(buffer);
    int error = cJSON_GetNumberValue(cJSON_GetObjectItem(json_reply, "error"));
    cJSON_Delete(json_reply);
    cJSON_Delete(root);
    return error;
}

CSHConfig csh_GetConfig() {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "mode", cJSON_CreateString("getConfig"));
    char *json_data = cJSON_PrintUnformatted(root);
    fifo_send(json_data);

    char buffer[10000];
    read(module->pipe, buffer, sizeof(buffer));
    printf("%s\n", buffer);
    CSHConfig config;
    cJSON_Delete(root);
    return config;
}

void csh_SetConfig(CSHConfig config) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "mode", cJSON_CreateString("getConfig"));
    cJSON_AddItemToObject(root, "data", CSHConfigtoJSON(config));
    char *json = cJSON_PrintUnformatted(root);
    fifo_send(json);
    cJSON_Delete(root);
}



void fifo_send(char *str) {
    write(module->pipe, str, strlen(str));
    sleep(0.1);
}