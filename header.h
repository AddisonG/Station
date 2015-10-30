#ifndef HEADER_FILE
#define HEADER_FILE

#include <netdb.h>
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>

#define MAXHOSTNAMELEN 128

// For establishConnection
#define OUR_REQUEST 0
#define THEIR_REQUEST 1

// For the "reason" parameter of "logShutdown" function
#define NOREASON 0
#define DOOMTRAIN 1
#define STOPSTATION 2

typedef struct Resource {
    char* name;
    int quantity;
    struct Resource* next;
} resource;

typedef struct Station {
    char* name;
    char* auth; // AKA: secret
    FILE* logFile;
    int port;
    char* hostname; // AKA: interface
    FILE* toStation; // Not for 'this'
    FILE* fromStation; // Not for 'this'
    
    int numProcessed;
    int numWrongStation;
    int numBadFormat;
    int numBadDestination;
    
    struct Station* next; // Start of linked list of connected stations
    resource* resources;
} station;

typedef struct Connection {
    station* this;
    station* connected;
} connection;

void hangUpHandler(int value);
station* establishConnection(station* this, char* hostname, int port,
    int fd, int type);
void* handleRequest(void* parameter);
void* handleResponse(void* parameter);
int processTrain(station* this, station* other);
char* addStation(station* this, char* train);
char* processResource(station* this, char* train);
void handleDoom(station* this);
void logShutdown(station* this, int reason);
void error(int error);

int open_listen(int port, char* hostname) {
    // Create IPv4 socket
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
	error(5);
    }

    // Allow address (IP addr + port num) to be reused
    int optVal = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &optVal, sizeof(int)) < 0) {
	error(5);
    }

    // Set up address structure for the server address
    // IPv4, given port number (network byte order), any IP address
    // on this machine
    struct sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);

    if (hostname != NULL) {
	struct hostent* host = gethostbyname(hostname);
	serverAddr.sin_addr.s_addr = htonl(*(long*)host->h_addr_list[0]);
    } else {
	serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    }

    // Bind to this address
    
    if (bind(fd, (struct sockaddr*)&serverAddr,
	sizeof(struct sockaddr_in)) < 0) {
	error(5);
    }

    // Ready to accept connections
    // SOMAXCONN is max requests from OS (128)
    socklen_t fd_len = sizeof(struct sockaddr_in);
    getsockname(fd, (struct sockaddr *)&serverAddr, &fd_len);
    printf("%d\n", ntohs(serverAddr.sin_port));
    
    if (listen(fd, SOMAXCONN) < 0) {
	error(5);
    }
    return fd;
}

#endif