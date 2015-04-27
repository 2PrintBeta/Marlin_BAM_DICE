#include <user_config.h>
#include <SmingCore/SmingCore.h>
#include "../include/configuration.h"
#include "../include/webserver.h"
#include "../include/arduino_com.h"


Timer cmdTimer;

void sendAPmode()
{
	ActiveConfig.ip = WifiAccessPoint.getIP().toString();
	ActiveConfig.mode = "AP";
}

void sendSTATIONmode()
{
	ActiveConfig.ip = WifiStation.getIP().toString();
	ActiveConfig.mode = "STATION";
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
	Serial.setTimeout(500);
	Serial.systemDebugOutput(false); // Enable debug output to serial

	//start wireless
	ActiveConfig = loadConfig();
	if(ActiveConfig.mode == "STATION")
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
	cmdTimer.initializeMs(500, handle_ardunio).start();

}
