#include <string.h>
#include <stdbool.h>
#include "../lib/cJSON/cJSON.h"
#include "ANSILib.h"
#include <sys/socket.h>
#include <arpa/inet.h>

#define MAX_NAME 100
#define MAX_ERROR 100
#define MAX_EVENT 40
#define MAX_EVENTS 20
#define MAX_COMMAND 20
#define MAX_COMMANDS 30
#define MAX_ALIASES_STRING 20
#define MAX_ALIASES 10
#define MAX_ARGS_STRING 20
#define MAX_ARGS 20
#define MAX_COMMAND_DESCRIPTION 200

#define MAX_MANPAGE_TITLE 100
#define MAX_MANPAGE_TEXT 100000
#define MAX_MANPAGE_COLOR 10
#define MAX_MANPAGE_PARAGRAPHS 30

#define MAX_MODULE_NAME 20
#define MAX_MODULE_VERSION 10
#define MAX_MODULE_DESCRIPTION 200
#define MAX_MODULE_AUTHOR 30
#define MAX_MODULES 20

#define MAX_PATH 200
#define MAX_LIBARY_NAME 40
#define MAX_LIBARIES 30

#define NSSH_BUFFER_SIZE 100000
#define MAX_NSSHCONFIG_BLACKLIST 1000
#define MAX_IP 30

typedef char *(*OnInitFunc)();
typedef char *(*OnExecutionFunc)(char *, char **, int);
typedef char *(*OnEventFunc)(cJSON *);

typedef struct {
    char version[MAX_MODULE_VERSION];
    char name[MAX_MODULE_NAME];
    char description[MAX_MODULE_DESCRIPTION];
    char author[MAX_MODULE_AUTHOR];
} CSHModuleInfo;

typedef struct {
    char event[MAX_EVENT];
    OnEventFunc onEvent;
} ModuleEvent;

typedef struct {
    char Text[MAX_MANPAGE_TITLE];
    char Color[MAX_MANPAGE_COLOR];
    int Bold;
    int Italic;
    int Crossout;
    int Underscore;
} ManpageTitle;

typedef struct {
    char Text[MAX_MANPAGE_TEXT];
    char Color[MAX_MANPAGE_COLOR];
    int Bold;
    int Italic;
    int Crossout;
    int Underscore;
} ManpageContent;

typedef struct {
    ManpageTitle *Title;
    ManpageContent *Content;
} ManpageParagraph;

typedef struct {
    ManpageTitle *Title;
    ManpageParagraph *Paragraphs;
    int paragraph_count;
} CSHManpage;

typedef struct {
    char command[MAX_COMMAND];
    char aliases[MAX_ALIASES_STRING][MAX_ALIASES];
    int alias_count;
    char description[MAX_COMMAND_DESCRIPTION];
    OnExecutionFunc onExecution;
    CSHManpage *manpage;
} ModuleCommand;

typedef struct {
    ModuleCommand commands[MAX_COMMANDS];
    ModuleEvent events[MAX_EVENTS];
    int command_count;
    int event_count;
    int pipe;
    OnInitFunc onInit;
    OnExecutionFunc onExecution;
    CSHModuleInfo Info;
    char *folder_path;
    char *executable_path;
} CSHModule;

typedef struct {
    char name[30];
    char mac[18];
} NSSHBannedClient;

typedef struct {
    bool enabled;
    int port;
    NSSHBannedClient banned_clients[MAX_NSSHCONFIG_BLACKLIST];
    int banned_clients_count;
} NSSHConfig;

typedef struct {
    char folder_path[MAX_PATH];
    char libary_name[MAX_LIBARY_NAME];
} CSHLibary;

typedef struct {
    NSSHConfig nssh_config;
    CSHLibary libarys[MAX_LIBARIES];
    int libary_count;
    char disabled_modules[MAX_MODULE_NAME][MAX_MODULES];
    int disabled_modules_count;
} CSHConfig;

void csh_InitModule(char **args, int argc);
void csh_BuildModule(char *name, char *author, char *version, char *description, OnInitFunc onInit);
void csh_RegisterCommand(char *command, char *aliases[], int alias_count, char *description, CSHManpage *manpage, OnExecutionFunc onExecution);
void csh_RegisterEvent(char *event, OnEventFunc onEvent);
CSHModule *csh_GetModule();

CSHManpage *Manpage_Init();
ManpageParagraph *Manpage_CreateParagraph();
ManpageTitle *Manpage_CreateTitle(char *titleStr);
ManpageContent *Manpage_CreateContent(char *contentStr);

void Manpage_SetTitle(CSHManpage *manpage, ManpageTitle *title);
void Manpage_AddParagraph(CSHManpage *manpage, ManpageParagraph *paragraph);
void Manpage_SetParagraphContent(ManpageParagraph *paragraph, ManpageContent *content);
void Manpage_SetParagraphTitle(ManpageParagraph *paragraph, ManpageTitle *title);
void Manpage_SetContentFormat(ManpageContent *Content, char *color, int bold, int italic, int crossout, int underscore);
void Manpage_SetTitleFormat(ManpageTitle *Title, char *color, int bold, int italic, int crossout, int underscore);
void Manpage_Release(CSHManpage *manpage);

void csh_printNormal(const char *format, ...);
void csh_printInfo(const char *format, ...);
void csh_printError(const char *format, ...);
void csh_printFatal(const char *format, ...);
void csh_printJSON(cJSON *json);

cJSON *CSHConfigtoJSON(CSHConfig config);
CSHConfig CSHConfigfromJSON(cJSON *json);
CSHManpage *ManpageFromJSON(cJSON *json);
cJSON *ManpageToJSON(CSHManpage *manpage);

CSHConfig csh_GetConfig();
void csh_SetConfig(CSHConfig config);

void csh_callEvent(char *event, cJSON *data);
void csh_Shutdown(int error_code);
int csh_ExecuteCommand(char *str, bool silent_mode);