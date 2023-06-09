#include <stddef.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include <openssl/ssl.h>
#include <openssl/err.h>

#define MAX_THREADS 30

typedef struct {
    struct sockaddr_in client;
    int socket;
    int error;

    SSL_CTX* ctx;
    SSL* ssl;
    
    int port;
    char hostname[100];
    char mac[18];
    int running;
} NSSHClient;

int WriteMessage(NSSHClient *client, char *message);
int ReadMessage(NSSHClient *client, char *message);

NSSHClient *nsshClient_Initialize(char *ip, int port);
int nsshClient_Connect(NSSHClient *client);
int nsshClient_SendMessage(NSSHClient *client, char *message);
int nsshClient_ReciveMessage(NSSHClient *client, char *message);
int nsshClient_Close(NSSHClient *client);

int isClientConnected(NSSHClient *client);

SSL_CTX* createSSLContext();