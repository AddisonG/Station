#ifndef HEADER_FILE
#define HEADER_FILE

#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <pthread.h>
#include <sys/types.h>

// For the "quantity" member of the "resource" struct
#define LOADED 0
#define UNLOADED 1

// For the "reason" parameter of "logShutdown" function
#define NOREASON 0
#define DOOMTRAIN 42
#define STOPTRAIN 9001


typedef struct Station {
	char* name;
	char* auth;
	FILE* logFile;
	int port;
	char* hostname; // AKA: interface
	
	int numProcessed;
	int numWrongStation;
	int numBadFormat;
	int numBadDestination;
	
	resource* resources; // lexicographical order
} station;

typedef struct Resource {
	char* name;
	int quantity[2];
	resource* next;
} resource;

void handleTrain(station* this, char* train);
void addStation(station* this, char* train);
void logShutdown(station* this, int reason);
void error(int error);

#endif