#include "NSSHLib.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>

int WriteMessage(NSSHClient *data, char *message) {
    int error = SSL_write(data->ssl , message , strlen(message));
    if (error <= 0) {
        return error;
    }
    return 1;
}

int ReadMessage(NSSHClient *data, char *message) {
    int read_size = SSL_read(data->ssl, message, 1000);
    if (read_size <= 0)
    {
        return read_size;
    }
    return 1;
}



NSSHClient *nsshClient_Initialize(char *ip, int port) {
    char message[1000];
    NSSHClient *data = malloc(sizeof(NSSHClient));
    // Prepare the sockaddr_in structure
    data->client.sin_family = AF_INET;
    data->client.sin_port = htons(port);
    data->port = port;

    if (inet_pton(AF_INET, ip, &(data->client.sin_addr)) <= 0) {
        perror("Ungültige IP-Adresse");
        return NULL;
    }

    // Create a socket
    data->socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    // Hostnamen abrufen
    if (gethostname(data->hostname, sizeof(data->hostname)) != 0) {
        perror("Fehler beim Abrufen des Hostnamens");
        return NULL;
    }

    FILE* fp;
    fp = popen("ifconfig eth0 | grep -o -E '([[:xdigit:]]{1,2}:){5}[[:xdigit:]]{1,2}'", "r");
    if (fp == NULL) {
        perror("Pipe-Fehler");
        return NULL;
    }
    fgets(data->mac, sizeof(data->mac), fp);
    pclose(fp);
    return data;
}

int nsshClient_Connect(NSSHClient *client) {
    // Connect to the server
    int error = connect(client->socket, (struct sockaddr *)&client->client , sizeof(client->client));
    if (error < 0)
    {
        return 0;
    }

    // Erstelle den SSL-Kontext
    client->ctx = createSSLContext();

    // Erstelle die SSL-Verbindung
    client->ssl = SSL_new(client->ctx);
    SSL_set_fd(client->ssl, client->socket);

    // Verbinde die SSL-Verbindung zum Server
    if (!SSL_connect(client->ssl)) {
        perror("SSL_connect");
        ERR_print_errors_fp(stderr);
        return 0;
    }
    return 1;
}

int nsshClient_SendMessage(NSSHClient *client, char *message) {
    int error = WriteMessage(client, message);
    return error;
}

int nsshClient_ReciveMessage(NSSHClient *client, char *message) {
    memset(message, 0, sizeof(message));
    int error = ReadMessage(client, message);
    return error;
}

int nsshClient_Close(NSSHClient *client) {
    SSL_shutdown(client->ssl);
    SSL_free(client->ssl);
    close(client->socket);
    SSL_CTX_free(client->ctx);
    free(client);
}



SSL_CTX* createSSLContext() {
    SSL_library_init();
    SSL_CTX* ctx = SSL_CTX_new(SSLv23_client_method());
    if (ctx == NULL) {
        perror("SSL_CTX_new");
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }
    return ctx;
}