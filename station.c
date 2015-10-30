#include "header.h"

static volatile bool interrupted = false;

void hangUpHandler(int value) {
    interrupted = true;
}

int main(int argc, char *argv[]) {
    
    struct sigaction act;
    act.sa_handler = hangUpHandler;
    sigaction(SIGHUP, &act, NULL);
    
    station this;
    memset(&this, 0, sizeof(station));
    
    if (argc < 4 || argc > 6) {
	error(1);
    }
    
    this.name = argv[1];
    
    FILE *auth = fopen(argv[2], "r");
    char buffer[8192];
    fgets(buffer, sizeof(buffer), auth); // leave the newline on it
    if (strlen(buffer) == 0) {
	error(2);
    }
    this.auth = &(buffer[0]);

    this.logFile = fopen(argv[3], "a");
    if (this.logFile == NULL) {
	error(3);
    }
    
    if (argc > 4) {
	this.port = atoi(argv[4]);
	if (this.port < 1 || this.port > 65535) {
	    error(4);
	}
	if (argc == 6) {
	    this.hostname = argv[5];
	}
    } else {
	this.port = 0;
    }

    int socket = open_listen(this.port, this.hostname);
    while (!interrupted) {
	socklen_t fromAddrSize = sizeof(struct sockaddr_in);
	// Block, wait for new connection
	// (fromAddr will be populated with client address details)
	
	struct sockaddr_in fromAddr;
	int fd = accept(socket, (struct sockaddr*)&fromAddr, &fromAddrSize);
	if (interrupted) {
	    logShutdown(&this, NOREASON);
	    exit(0);
	}
	if (fd < 0) {
	    error(5);
	}
	
	// Convert IP into hostname
	char hostname[MAXHOSTNAMELEN];
	if (getnameinfo((struct sockaddr*)&fromAddr, fromAddrSize, hostname,
	    MAXHOSTNAMELEN, NULL, 0, 0)) {
	    error(6);
	}

	// Start a new thread to deal with client communication
	// Pass the connected file descriptor as an argument to
	// the thread (cast to void*)
	
	connection* conn = malloc(sizeof(connection));
	conn->this = &this;
	conn->connected = establishConnection(&this, hostname,
		ntohs(fromAddr.sin_port), fd, THEIR_REQUEST);
	
	pthread_t threadId;
	pthread_create(&threadId, NULL, handleRequest, conn);
	pthread_detach(threadId);
    }
    
    logShutdown(&this, NOREASON);
}

station* establishConnection(station* this, char* hostname, int port,
	int fd, int type) {
    station* conn = malloc(sizeof(station));
    conn->toStation = fdopen(fd, "w");
    conn->fromStation = fdopen(fd, "r");

    conn->hostname = hostname;
    conn->port = port;

    if (type == OUR_REQUEST) {
	fprintf(conn->toStation, "%s%s\n", this->auth, this->name);
    } else { // THEIR_REQUEST
	fprintf(conn->toStation, "%s\n", this->name);
    }
    fflush(conn->toStation);

    return conn;
}

// Just checks secret first
void* handleRequest(void* parameter) {
    connection* conn = (connection*) parameter;
    station* other = conn->connected;
    station* this = conn->this;

    char buffer[1024];
    
    // Compare secret
    fgets(buffer, 1024, other->fromStation);
    if (strcmp(buffer, this->auth)) {
	// Bad secret
	return NULL;
    }
    handleResponse(parameter);
    return NULL;
}

void* handleResponse(void* parameter) {
    connection* conn = (connection*) parameter;
    station* other = conn->connected;
    station* this = conn->this;
    
    char buffer[1024];
    fgets(buffer, 1024, other->fromStation);
    buffer[strlen(buffer) - 1] = '\0'; // remove newline
    other->name = malloc(sizeof(char) * (strlen(buffer) + 1));
    strcpy(other->name, buffer);

    // add into the list
    if (this->next == NULL) { // First connection ever
	this->next = other;
	other->next = NULL;
    } else {
	station *previous = NULL, *current = this->next;
	while (current != NULL && strcmp(other->name, current->name) > 0) {
	    previous = current;
	    current = current->next;
	}
	if (current != NULL && !strcmp(other->name, current->name)) {
	    // Same name - error
	    error(7);
	} else { // Insert connection
	    if (previous == NULL) { // Start of list
		this->next = other;
	    } else {
		previous->next = other;
	    }
	    other->next = current;
	}
    }

    int value = 1;
    while (value) {
	value = processTrain(this, other);
	if (value == -1) {
	    return NULL;
	}
    }

    logShutdown(this, STOPSTATION);
    exit(0);
    return NULL;
}

int processTrain(station* this, station* other) {
    char* train = malloc(sizeof(char) * 65535);
    if (NULL == fgets(train, sizeof(char) * 65535, other->fromStation)
	    || train == NULL) { // Erase this from the connections
	station *previous = NULL, *current = this->next;
	while (strcmp(other->name, current->name)) {
	    previous = current;
	    current = current->next;
	}
	if (previous == NULL) { // Start of list
	    this->next = current->next;
	} else {
	    previous->next = current->next;
	}
	return -1;
    }
    char *name = strsep(&train, ":");
    if (strcmp(this->name, name)) {
	this->numWrongStation++; // Wrong station name, discard
	return 1;
    }
    if (train == strstr(train, "doomtrain:") || train ==
	    strstr(train, "doomtrain\n")) {
	handleDoom(this);
	logShutdown(this, DOOMTRAIN);
	exit(0);
    }
    int keepGoing = 1;
    if (train == strstr(train, "stopstation")) {
	train += strlen("stopstation");
	keepGoing = 0;
    } else if (train == strstr(train, "add(")) {
	train += strlen("add(");
	train = addStation(this, train); // Must be at least 1
	while (train[0] == ',') {
	    train += strlen(",");
	    train = addStation(this, train);
	}
	if (train[0] != ')') { // Bad train, discard
	    this->numBadFormat++;
	    return 1;
	}
	train += strlen(")");
    } else {
	train = processResource(this, train); // Must be at least 1
	while (train[0] == ',') {
	    train += strlen(",");
	    train = processResource(this, train);
	}
    }
    if (train[0] == '\n' && train[1] == '\0') { // Forward train
	this->numProcessed++; // End of train
	return 1;
    } else if (train[0] == ':') {
	train += strlen(":");
    } else {
	this->numBadFormat++;
	return 1;
    }
    char* destination = strsep(&train, ":");
    if (!strcmp(destination, this->name)) {
	// pointing to myself! Not allowed
	this->numBadDestination++;
	return 1;
    }
    station* pointer = this->next;
    while (pointer != NULL && strcmp(pointer->name, destination)) {
	pointer = pointer->next;
    }
    if (pointer == NULL || pointer->toStation == NULL) {
	this->numBadDestination++;
	return 1;
    }
    fprintf(pointer->toStation, "%s:%s", destination, train);
    fflush(pointer->toStation);
    this->numProcessed++;
    return keepGoing;
}

char* addStation(station* this, char* train) {
    int port;
    char hostname[MAXHOSTNAMELEN], str[MAXHOSTNAMELEN + 6]; // +room for port#

    if (2 != sscanf(train, "%d@%[a-zA-Z0-9-]", &(port), hostname)) {
	// Didn't match both port and host
	// DONT INCREMENT TRAIN COUNT HERE
	return "";
    }
    sprintf(str, "%d@%s", port, hostname);
    train += strlen(str);

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
	error(6);
    }

    struct sockaddr_in outgoing_address;
    memset(&outgoing_address, 0, sizeof(outgoing_address));
    outgoing_address.sin_family = AF_INET;
    outgoing_address.sin_port = htons(port);
    struct hostent* host = gethostbyname(hostname);
    outgoing_address.sin_addr.s_addr = *(long*)host->h_addr_list[0];

    if (connect(fd, (struct sockaddr*)&outgoing_address,
	    sizeof(outgoing_address))) {
	error(6);
    }

    connection* conn = malloc(sizeof(connection));
    conn->this = this;
    conn->connected = establishConnection(this, hostname, port,
	    fd, OUR_REQUEST);
    
    pthread_t threadId;
    pthread_create(&threadId, NULL, handleResponse, conn);
    pthread_detach(threadId);
    return train;
}

char* processResource(station* this, char* train) {
    char resourceName[1024], str[1060];
    char sign[16];
    int quantity;

    if (3 != sscanf(train, "%[^:+-]%[+-]%d", resourceName, sign,
	    &quantity)) {
	// Didn't match both all 3 parameters
	this->numBadFormat++;
	return "";
    }
    sprintf(str, "%d%s%s", quantity, sign, resourceName);
    train += strlen(str);

    if (strlen(sign) > 1) {
	// Can only have 1 sign
	this->numBadFormat++;
	return "";
    }
    resource* res;
    if (this->resources == NULL) { // First resource ever
	res = malloc(sizeof(resource));
	res->name = malloc(sizeof(char) * (strlen(resourceName) + 1));
	strcpy(res->name, resourceName);
	res->next = NULL;
	res->quantity = 0;
	this->resources = res;
    } else {
	resource *previous = NULL, *current = this->resources;
	while (current != NULL && strcmp(resourceName, current->name) > 0) {
	    previous = current;
	    current = current->next;
	}
	
	if (current != NULL &&
		!strcmp(resourceName, current->name)) { // Loading old resource
	    res = current;
	} else { // Inserting new resource
	    res = malloc(sizeof(resource));
	    res->name = malloc(sizeof(char) * (strlen(resourceName) + 1));
	    strcpy(res->name, resourceName);
	    if (previous == NULL) { // Start of list
		this->resources = res;
	    } else {
		previous->next = res;
	    }
	    res->next = current;
	    res->quantity = 0;
	}
    }

    if (sign[0] == '+') {
	res->quantity += quantity;
    } else {
	res->quantity -= quantity;
    }
    return train;
}

void handleDoom(station* this) {
    station* pointer = this->next;
    while (pointer != NULL) {
	fprintf(pointer->toStation, "%s:doomtrain\n", pointer->name);
	fflush(pointer->toStation);
	pointer = pointer->next;
    }
}

void logShutdown(station* this, int reason) {
    FILE* logFile = this->logFile;
    
    fprintf(logFile, "=======\n");
    fprintf(logFile, "%s\n", this->name);
    fprintf(logFile, "Processed: %d\n", this->numProcessed);
    fprintf(logFile, "Not mine: %d\n", this->numWrongStation);
    fprintf(logFile, "Format err: %d\n", this->numBadFormat);
    fprintf(logFile, "No fwd: %d\n", this->numBadDestination);
    
    // print connections
    if (this->next != NULL) {
	fprintf(logFile, "%s", this->next->name);
	station* pointer = this->next->next;
	while (pointer != NULL) {
	    fprintf(logFile, ",%s", pointer->name);
	    pointer = pointer->next;
	}
	fprintf(logFile, "\n");
    } else {
	fprintf(logFile, "NONE\n");
    }

    // print resources
    resource* resPointer = this->resources;
    while (resPointer != NULL) {
	fprintf(logFile, "%s %d\n", resPointer->name, resPointer->quantity);
	resPointer = resPointer->next;
    }
    
    if (reason == DOOMTRAIN) {
	fprintf(logFile, "doomtrain\n");
    } else if (reason == STOPSTATION) {
	fprintf(logFile, "stopstation\n");
    }
    fflush(logFile);
}

void error(int error) {
    switch (error) {
	case 1:
	    fprintf(stderr,
		    "Usage: station name authfile logfile [port [host]]\n");
	    exit(error);
	case 2:
	    fprintf(stderr, "Invalid name/auth\n");
	    exit(error);
	case 3:
	    fprintf(stderr, "Unable to open log\n");
	    exit(error);
	case 4:
	    fprintf(stderr, "Invalid port\n");
	    exit(error);
	case 5:
	    fprintf(stderr, "Listen error\n");
	    exit(error);
	case 6:
	    fprintf(stderr, "Unable to connect to station\n");
	    exit(error);
	case 7:
	    fprintf(stderr, "Duplicate station names\n");
	    exit(error);
	case 99:
	    fprintf(stderr, "Unspecified system call failure\n");
	    exit(error);
	case 0:
	    exit(error);
    }
}