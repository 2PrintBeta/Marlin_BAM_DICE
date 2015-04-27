#ifndef INCLUDE_WEBSERVER_H_
#define INCLUDE_WEBSERVER_H_

#define MAX_FILENAMES 50
struct BAMState
{
	String temp1;
	String temp1Target;
	String temp2;
	String temp2Target;
	String tempBed;
	String tempBedTarget;
	String xPos;
	String yPos;
	String zPos;
	String SDselected;
	String SDpercent;
	String printTime;

	bool SDinserted;
	uint16_t numSDEntries;
	String SDEntries[MAX_FILENAMES];
};

extern BAMState curState;
extern bool uploadInProgress;

void startWebServer();
void startFTP();


#endif
