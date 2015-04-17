#include <user_config.h>
#include <SmingCore/SmingCore.h>
#include "../include/configuration.h"
#include "../include/webserver.h"

Timer cmdTimer;
#define CMD_LEN 200
char cmd_buf[CMD_LEN];
int cmd_pos=0;
void parseCmd()
{
	bool cmd_available = false;
	while(Serial.available())
	{
		// get char
		cmd_buf[cmd_pos] = Serial.read();

		//check for end of cmd
		if(cmd_buf[cmd_pos] == '\n')
		{
			cmd_buf[cmd_pos+1] = 0;
			cmd_available = true;
			break;
		}
		//increment char pos
		cmd_pos++;
		//check for overflow !
		if(cmd_pos > CMD_LEN)
		{
			cmd_pos=0;
			//Error too long command
			// TODO error handling ?
			break;
		}
	}

	//process commands
	if(cmd_available)
	{
		if(strncmp(cmd_buf,"SEC",3)==0)
		{
			cmd_buf[cmd_pos-1] = 0;
			ActiveConfig.security = cmd_buf+4;
			saveConfig(ActiveConfig);
			Serial.println("ok");
		}
		else if(strncmp(cmd_buf,"MODE",4)==0)
		{
			if(strncmp(cmd_buf+5,"STATION",7)==0) ActiveConfig.isStation="yes";
			else ActiveConfig.isStation="no";
			saveConfig(ActiveConfig);
			Serial.println("ok");
		}
		else if(strncmp(cmd_buf,"SSID",4)==0)
		{
			cmd_buf[cmd_pos-1] = 0;
			ActiveConfig.NetworkSSID = cmd_buf+5;
			saveConfig(ActiveConfig);
			Serial.println("ok");
		}
		else if(strncmp(cmd_buf,"PWD",3)==0)
		{
			cmd_buf[cmd_pos-1] = 0;
			ActiveConfig.NetworkPassword = cmd_buf+4;
			saveConfig(ActiveConfig);
			Serial.println("ok");

		}
		else if(strncmp(cmd_buf,"RESET",5)==0)
		{
			Serial.println("ok");
			System.restart();
		}

		//reset cmd
		cmd_pos =0;
	}
}

void sendAPmode()
{
	//advertise IP
	Serial.print("IP ");
	Serial.println(WifiAccessPoint.getIP());

	//advertise MODE
	Serial.print("MODE ");
	Serial.println("AP");

	//advertise SSID
	Serial.print("SSID ");
	Serial.println(ActiveConfig.NetworkSSID);
}

void sendSTATIONmode()
{
	//advertise IP
	Serial.print("IP ");
	Serial.println(WifiStation.getIP());

	//advertise MODE
	Serial.print("MODE ");
	Serial.println("STATION");

	//advertise SSID
	Serial.print("SSID ");
	Serial.println(ActiveConfig.NetworkSSID);
}

// Will be called when WiFi station was connected to AP
void connectOk()
{
	startFTP();
	startWebServer();

	sendSTATIONmode();
}

//will be called if there is no connection after 20 seconds
void connectFail()
{
	//start AP
	WifiAccessPoint.config("BAM&DICE", "", AUTH_OPEN);
	WifiAccessPoint.enable(true);

	//start services
	startFTP();
	startWebServer();


	sendAPmode();

	// Wait connection in Station mode
	WifiStation.waitConnection(connectOk);
}

void init()
{
	Serial.begin(1000000); // 115200 by default
	//Serial.setTimeout(1000);
	Serial.systemDebugOutput(false); // Enable debug output to serial

	//start wireless
	ActiveConfig = loadConfig();
	if(ActiveConfig.isStation == "yes")
	{
		// set up station
		WifiStation.config(ActiveConfig.NetworkSSID, ActiveConfig.NetworkPassword);
		WifiStation.enable(true);
		WifiAccessPoint.enable(false);
		WifiStation.waitConnection(connectOk, 30, connectFail);
	}
	else
	{
		// set up accesspoint mode
		WifiStation.enable(false);
		AUTH_MODE mode = AUTH_OPEN;
		if (ActiveConfig.security == "WEP") mode = AUTH_WEP;
		else if (ActiveConfig.security == "WPA_PSK") mode = AUTH_WPA_PSK;
		else if (ActiveConfig.security == "WPA2_PSK") mode = AUTH_WPA2_PSK;
		else if (ActiveConfig.security == "WPA_WPA2_PSK") mode = AUTH_WPA_WPA2_PSK;
		else mode = AUTH_OPEN;

		WifiAccessPoint.config(ActiveConfig.NetworkSSID, ActiveConfig.NetworkPassword,mode);
		WifiAccessPoint.enable(true);

		// start services
		startFTP();
		startWebServer();

		System.onReady(sendAPmode);
	}

	// start cmd interface
	cmdTimer.initializeMs(500, parseCmd).start();


}
