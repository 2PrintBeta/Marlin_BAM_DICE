/*
  esp8266.c - support code for the esp8266 wlan modue
  Part of Marlin

  Copyright (c) 2015 Dominik Wenger

  Marlin is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Marlin is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with Marlin.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "esp8266.h"
#include "Configuration.h"

#ifdef HAVE_ESP8266

#define TIMEOUT 5000

bool esp8266_inited = false;
char WIFI_IP_Adress[20];

void RECV_Buffer_Clear() {
  while (Serial1.available())Serial1.read();
}

bool esp8266_check(bool output)
{
	String ret=""; 
	char ret_c;
    long start_time = millis();
	do 
	{
		if(Serial1.available() ) 
		{
			ret_c = Serial1.read();
			if((ret_c > '!') && (ret_c < '~'))  ret.concat(ret_c);
		
			//found OK
			if(ret.indexOf("OK") != -1)  return true;
	
			//found error
			if(ret.indexOf("no change") != -1) 
			{
				if(output) MYSERIAL.println(ret);
				return false;
			}
			if(ret.indexOf("Error") != -1) 
			{
				if(output) MYSERIAL.println(ret);
				return false;
			}
			if(ret.indexOf("no this fun") != -1) 
			{
				if(output) MYSERIAL.println(ret);
				return false;
			}
		}
		
	} while(millis() - start_time < TIMEOUT);
	
	if(output) 
	{
		MYSERIAL.print("Timeout! :");
		MYSERIAL.println(ret);
	}
	return false;
}

bool esp8266_send(char* text,bool output)
{
	Serial1.println(text);
	delay(10);
	if (!esp8266_check(output)) 
	{
		if(output)
		{
			MYSERIAL.print(F("Could not send:"));
			MYSERIAL.println(text);
		}
		esp8266_inited = false;
		RECV_Buffer_Clear();
		return false;
	}
	RECV_Buffer_Clear();
	return true;
}

bool esp8266_send2(char* text,int value,bool output)
{
	Serial1.print(text);
	Serial1.println(value);
	delay(10);
	if (!esp8266_check(output)) 
	{
		if(output)
		{
			MYSERIAL.print(F("Could not send: "));
			MYSERIAL.print(text);
			MYSERIAL.println(value);
		}
		esp8266_inited = false;
		RECV_Buffer_Clear();
		return false;
	}
	RECV_Buffer_Clear();
	return true;
}

bool esp8266_send3(char* text,char* text2,char* text3,bool output)
{
	Serial1.print(text);
	Serial1.print(text2);
	Serial1.print(",");
	Serial1.println(text3);
	delay(10);
	if (!esp8266_check(output)) 
	{
		if(output)
		{
			MYSERIAL.print(F("Could not send:"));
			MYSERIAL.println(text);
			MYSERIAL.print(text2);
			MYSERIAL.print(",");
			MYSERIAL.println(text3);
		}
		RECV_Buffer_Clear();
		esp8266_inited = false;
		return false;
	}
	RECV_Buffer_Clear();
	return true;
}



void init_esp8266()
{
    //check if connection is possible
    Serial1.begin(119200);
	Serial1.setTimeout(TIMEOUT);
	// TEST connection
	if(!esp8266_send("AT",false))
	{
		// no connection possible - Try to change baudrate
	    Serial1.end();
	    Serial1.begin(9600);
	    Serial1.setTimeout(TIMEOUT);
		
		// TEST connection
		if(!esp8266_send("AT",true))	return;
		
		//change baudrate to 119200
		if(!esp8266_send("AT+BAUD=119200",true)) return;
		
		Serial1.end();
	    Serial1.begin(119200);
	    Serial1.setTimeout(TIMEOUT);
	   	// TEST connection
		if(!esp8266_send("AT",true)) return;
		
	}
		
	// set to multiple connection (else servermode does not work)
	if(!esp8266_send("AT+CIPMUX=1",true)) return;
	
	// enable server
	if(!esp8266_send2("AT+CIPSERVER=1,",ESP8266_PORT,false))
	{
		//try to stop existing server and restart
		Serial1.println("AT+CIPSERVER=0");
		delay(10);
		Serial1.println("AT+RST");
		delay(2000);
	    RECV_Buffer_Clear();
	    //setup multi connectio again
		if(!esp8266_send("AT+CIPMUX=1",true)) return;
		
		// try to start server again
		if(!esp8266_send2("AT+CIPSERVER=1,",ESP8266_PORT,true)) return;
	} 

	// Timeout für automatisches Trennen der Verbindung setzen
	if(!esp8266_send("AT+CIPSTO=0",true)) return;
		
	// check AP /STation mode
    Serial1.println(F("AT+CWMODE?"));
    delay(10);	
	int WIFI_CWMODE = 0;
	if (Serial1.find("AT+CWMODE?\r\r\n+CWMODE:")) {
		WIFI_CWMODE = Serial1.parseInt();
	}	
    RECV_Buffer_Clear();
	
	// AP / station configurieren
	if(WIFI_CWMODE != ESP8266_MODE)
		if(!esp8266_send2("AT+CWMODE=",ESP8266_MODE,false)) return;
		
	// connect to AP
	if(ESP8266_MODE == ESP_STATION)
	{
		if(!esp8266_send3("AT+CWJAP=",ESP8266_SSID,ESP8266_PWD,true)) return;
	}
	else  // set up AP
	{
	   // TODO
	}
	
	// get own IP address
    Serial1.println("AT+CIFSR");
    delay(1000);

    int len = 0;
    WIFI_IP_Adress[0] = 0;		
    if (Serial1.find("AT+CIFSR\r\r\n")) 
	{
		len = Serial1.readBytesUntil('\r', WIFI_IP_Adress, 20);
		WIFI_IP_Adress[len] = 0;
	}
    if (len > 0) 
	{
      MYSERIAL.print("Service is listening at: ");
      MYSERIAL.print(WIFI_IP_Adress);
	  MYSERIAL.print(":");
	  MYSERIAL.println(ESP8266_PORT);
    }
	else
	{
		MYSERIAL.println("Could not get my own IP address ! ");
		esp8266_inited = false;
		return;
	}
	
	esp8266_inited = true;
}




#endif
