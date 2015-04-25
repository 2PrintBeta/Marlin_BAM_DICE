#include <user_config.h>
#include <SmingCore/SmingCore.h>
#include "../include/configuration.h"
#include "../include/webserver.h"

Timer cmdTimer;

enum eCMDs
{
	eNETWORK,
	eTEMP,
	ePOS,
	eSD
};

eCMDs cmd_state = eNETWORK;

void parseCmd()
{
	// do not request new state while upload is running
	if(uploadInProgress) return;

	//request different states everytime
	String resp;
	bool network_changed = false;
	switch(cmd_state)
	{
		case eNETWORK:
			Serial.println("NETWORK");
			//wait for answer
			resp = Serial.readStringUntil('\n');
			resp.trim();
			if(resp.startsWith("SEC"))
			{
				if((resp.substring(4).length() > 0) &&
				   (ActiveConfig.security != resp.substring(4)))
				{
					ActiveConfig.security = resp.substring(4);
					network_changed = true;
				}
			}
			else return;

			Serial.println("ok");
			resp = Serial.readStringUntil('\n');
			resp.trim();
			if(resp.startsWith("MODE"))
			{
				if(resp.substring(5).length() > 0)
				{
					if(resp.substring(5).startsWith("STATION") && (ActiveConfig.isStation == "no"))
					{
						ActiveConfig.isStation = "yes";
						network_changed = true;
					}
					else if(resp.substring(5).startsWith("AP") && (ActiveConfig.isStation == "yes"))
					{
						ActiveConfig.isStation = "no";
						network_changed = true;
					}
				}
			}
			else return;

			Serial.println("ok");
			resp = Serial.readStringUntil('\n');
			resp.trim();
			if(resp.startsWith("SSID"))
			{
				if(resp.substring(5).length() > 0)
				{
					if(ActiveConfig.NetworkSSID != resp.substring(5))
					{
						ActiveConfig.NetworkSSID = resp.substring(5);
						network_changed = true;
					}
				}
			}
			else return;

			Serial.println("ok");
			resp = Serial.readStringUntil('\n');
			resp.trim();
			if(resp.startsWith("PWD"))
			{
				if(resp.substring(4).length() > 0)
				{
					if(ActiveConfig.NetworkPassword != resp.substring(4))
					{
						ActiveConfig.NetworkPassword = resp.substring(4);
						network_changed = true;
					}
				}
			}
			else return;

			// restart system if network changed
			if(network_changed)
			{
				Serial.println("restarting");
				saveConfig(ActiveConfig);
				System.restart();
			}
			cmd_state = eTEMP;
		break;
		case eTEMP:
			Serial.println("TEMP");
			resp = Serial.readStringUntil('\n');
			if(resp.startsWith("TEMP"))
			{
				// parse temp values
				resp = resp.substring(5);
				int startpos =0;
				int endpos = resp.indexOf(' ');
				curState.temp1 = resp.substring(startpos,endpos);
				startpos = endpos+1;
				endpos = resp.indexOf(' ',startpos);
				curState.temp1Target = resp.substring(startpos,endpos);
				startpos = endpos+1;
				endpos = resp.indexOf(' ',startpos);
				curState.temp2 = resp.substring(startpos,endpos);
				startpos = endpos+1;
				endpos = resp.indexOf(' ',startpos);
				curState.temp2Target = resp.substring(startpos,endpos);
				startpos = endpos+1;
				endpos = resp.indexOf(' ',startpos);
				curState.tempBed = resp.substring(startpos,endpos);
				startpos = endpos+1;
				endpos = resp.indexOf(' ',startpos);
				curState.tempBedTarget = resp.substring(startpos,endpos);
			}
			else return;
			cmd_state = ePOS;
		break;
		case ePOS:
			Serial.println("POS");
			resp = Serial.readStringUntil('\n');
			if(resp.startsWith("POS"))
			{
				// parse positions
				resp = resp.substring(4);
				int startpos =0;
				int endpos = resp.indexOf(' ');
				curState.xPos = resp.substring(startpos,endpos);
				startpos = endpos+1;
				endpos = resp.indexOf(' ',startpos);
				curState.yPos = resp.substring(startpos,endpos);
				startpos = endpos+1;
				endpos = resp.indexOf(' ',startpos);
				curState.zPos = resp.substring(startpos,endpos);
			}
			else return;
			cmd_state = eSD;
		break;
		case eSD:
			Serial.println("SD");
			resp = Serial.readStringUntil('\n');
			if(resp.startsWith("SD"))
			{
				//parse SD State
				resp = resp.substring(3);
				int startpos =0;
				int endpos = resp.indexOf(' ');
				curState.SDselected = resp.substring(startpos,endpos);
				startpos = endpos+1;
				endpos = resp.indexOf(' ',startpos);
				curState.SDpercent = resp.substring(startpos,endpos);
				startpos = endpos+1;
				endpos = resp.indexOf(' ',startpos);
				curState.printTime = resp.substring(startpos,endpos);
			}
			else return;
			cmd_state = eNETWORK;
		break;
		default:
			return;
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
	Serial.setTimeout(500);
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
