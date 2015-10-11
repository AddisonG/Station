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
	
	if (argc < 4 || argc > 6) {
		error(1);
	}
	
	this.name = argv[1];
	
	FILE *auth = fopen(argv[2], "r");
	char buffer[8192];
	fgets(this.auth, sizeof(buffer), auth);
	if (strlen(this.auth) == 0) {
		error(2);
	}
	
	this.logFile = fopen(argv[3], "w");
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
	}
	
	while (!interrupted) {
		//handleTrain(this, fgets());
		//forwardtrain
	}
	
	logShutdown(this, NOREASON);
}

void handleTrain(station* this, char* train) {
	char *name = train;
	train = strsep(&train, ":");
	
	if (strcmp(this->name, name)) {
		// Names are not the same
		
		// wrong station
		return;
	}
	
	if (train == strstr("doomtrain", train)) {
		logShutdown(this, DOOMTRAIN);
		exit(0);
	}
	
	if (train == strstr("stopstation", train)) {
		logShutdown(this, STOPSTATION);
		exit(0);
	}
	
	if (train == strstr("add(", train)) {
		train += strlen("add(");
		addStation(this, train); // Must be at least 1
		while (train[0] == ',') {
			train += strlen(",");
			addStation(this, train);
		}
		if (train[0] != ')') {
			// bad message
		}
		train += strlen(")");
		return;
	}
	
}

void addStation(station* this, char* train) {
	
}

void logShutdown(station* this, int reason) {
	FILE* logFile = this->logFile
	
	fprintf(logFile, "=======\n");
	
	fprintf(logFile, this->name);
	fprintf(logFile, "\n");
	
	fprintf(logFile, this->numProcessed);
	fprintf(logFile, "\n");
	
	fprintf(logFile, this->numWrongStation);
	fprintf(logFile, "\n");
	
	fprintf(logFile, this->numBadFormat);
	fprintf(logFile, "\n");
	
	fprintf(logFile, this->numBadDestination);
	fprintf(logFile, "\n");
	
	// print connections
	
	// print resources
	
	if (reason == DOOMTRAIN) {
		fprintf(logFile, "doomtrain\n");
	} else if (reason == STOPSTATION) {
		fprintf(logFile, "stopstation\n");
	}
}

void error(int error) {
	switch (error) {
		case 1:
			fprintf(stderr, "Usage: station name authfile logfile [port [host]]\n");
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