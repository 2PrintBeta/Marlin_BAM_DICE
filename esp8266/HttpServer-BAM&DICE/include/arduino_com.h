#ifndef ARDUINO_COM_H_
#define ARDUINO_COM_H_

#include <user_config.h>
#include <SmingCore/SmingCore.h>

enum ESP_CMDs
{
	//commands
	eSync=0,
	eSetTemp,
	eSetFan,
	eMove,
	eHome,
	ePrint,
	ePause,
	eResume,
	eStop,
	eDelete,
	eOpenFile,
	eCloseFile,
	eFileData,

	eGetNetworkSSID,
	eGetNetworkPWD,
	eGetNetworkMode,
	eGetNetworkSec,

	eGetNumSDEntries,
	eGetSDEntry,

	eSetNetworkSSID, //this commands return the current state
	eSetNetworkMode, //this commands return the current state
	eSetNetworkIP, //this commands return the current state

	// answers
	eOk,
	eError,

	//used for debug
	eDebug,
};

volatile extern bool cmd_failed;
extern String cmd_error_str;
volatile extern bool in_sync;

void handle_ardunio();

void sendActiveCmd(ESP_CMDs cmd,int len,unsigned char* data); //blocks till its sent, or timeout

#endif
