#ifndef INCLUDE_WEBSERVER_H_
#define INCLUDE_WEBSERVER_H_

#define MAX_FILENAMES 50
#define STORAGE_SIZE 15

struct BAMState
{
	float temp1;
	float temp1Target;
	float temp2;
	float temp2Target;
	float tempBed;
	float tempBedTarget;
	float xPos;
	float yPos;
	float zPos;
	bool SDselected;
	int SDpercent;
	unsigned long printTime;
	int fanSpeed;

	bool SDinserted;
	uint16_t numSDEntries;
	char SDEntries[MAX_FILENAMES][STORAGE_SIZE];
};

extern BAMState curState;
extern bool uploadInProgress;

void startWebServer();
void startFTP();


#endif
