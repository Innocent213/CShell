#include "../lib/CSHLib.h"
#include <stdio.h>
#include <stdbool.h>

void calculate(char *calculation, char *result) {
    int fistNum = 0; // Erste Nummer
    int secNum = 0; // Zweite Nummer
    int iresult = 0; // Ergebnis als Ineger
    char sign = -1;
    
    int j = 0; // Zählt, welche Ziffer gerade umgewandelt wird
    for (int i = 0; i < strlen(calculation); i++) {
        if (calculation[i] == '+' || calculation[i] == '-' || calculation[i] == '*' || calculation[i] == '/') {
            sign = calculation[i];
            j = 0;
        } else {
            if (sign == -1) {
                fistNum *= 10;
                fistNum += calculation[i]-'0';
            } else {
                secNum *= 10;
                secNum += calculation[i]-'0';
            }
            j++;
        }
    }

    if (sign == '+') {
        iresult = fistNum + secNum;
    }
    if (sign == '-') {
        iresult = fistNum - secNum;
    }
    if (sign == '*') {
        iresult = fistNum * secNum;
    }
    if (sign == '/') {
        iresult = fistNum / secNum;
    }

    // Konvertieren der Zahl in einen String
    int i = 0;
    while(iresult != 0) {
        int digit = iresult % 10;
        result[i++] = digit + '0';
        iresult /= 10;
    }

    // Umdrehen der Zahl
    int length = strlen(result);
    j = 0;
    for (i = 0, j = length-1; i < j; i++, j--) {
        char temp = result[i];
        result[i] = result[j];
        result[j] = temp;
    }
}

char *onInit() {
    // csh_ExecuteCommand("ls", false);
    return NULL;
}

char *onTestExecution(char *cmd, char *args[], int args_count) {
    char message[10000];
    csh_printNormal("test: %s\n", cmd);

    cJSON *data = cJSON_CreateObject();
    cJSON_AddItemToObject(data, "test", cJSON_CreateString("teststring"));

    csh_callEvent("testevent", data);
    // CSHConfig config = csh_GetConfig();
    return NULL;
}

char *onCalcExecution(char *cmd, char *args[], int args_count) {
    if (args_count == 1) {
        char result[10];
        calculate(args[0], result);
        csh_printNormal(result);
        csh_printNormal("\n\n");
    } else {
        return "Not the right amount of argumets!";
    }
    return NULL;
}

char *onShutdownExecution(char *cmd, char *args[], int args_count) {
    csh_printInfo("Shutting down...\n");
    csh_Shutdown(0);
}

char *onTestEvent(cJSON *data) {
    csh_printNormal("TestEvent: ");
    csh_printJSON(data);
    csh_printNormal("\n");
    return NULL;
}

int main(int argc, char *argv[]) {
    csh_InitModule(argv, argc);
    CSHManpage *manpage = Manpage_Init();
    ManpageParagraph *paragraph = Manpage_CreateParagraph();
    ManpageTitle *title = Manpage_CreateTitle("TestManpage");
    ManpageTitle *paragraph_title = Manpage_CreateTitle("TestParagraph");
    ManpageContent *paragraph_content = Manpage_CreateContent("Das ist ein Test Befehl!");

    Manpage_SetTitleFormat(title, COLOR_RED, true, true, true, false);
    Manpage_SetTitleFormat(title, COLOR_CYAN, false, false, false, false);
    Manpage_SetContentFormat(paragraph_content, COLOR_MAGENTA, false, false, false, false);

    Manpage_SetParagraphTitle(paragraph, paragraph_title);
    Manpage_SetParagraphContent(paragraph, paragraph_content);
    Manpage_AddParagraph(manpage, paragraph);
    Manpage_SetTitle(manpage, title);

    char *test_aliases[10] = { "te" };
    char *calc_aliases[10] = { "calc" };
    csh_RegisterCommand("test", test_aliases, 1, "Das ist ein Testbefehl.", manpage, onTestExecution);
    csh_RegisterCommand("calculate", calc_aliases, 1, "Dieser Befehl berechnet die Differenz/Summe/Quotient/Produkt zweier Zahlen aus.", NULL, onCalcExecution);
    csh_RegisterCommand("shutdown", NULL, 0, "Schaltet die Shell aus.", NULL, onShutdownExecution);

    csh_RegisterEvent("testevent", onTestEvent);

    csh_BuildModule("TestModul", "Author", "1.0", "Das ist ein Test Modul", onInit);
    return 0;
}