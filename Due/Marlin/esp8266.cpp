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
#include "ultralcd.h"
#include "temperature.h"
#include "cardreader.h"
#ifdef HAVE_ESP8266

#define wifi Serial1
#define TIMEOUT 5000
#define WIFI_BAUDRATE 9600
#define DEBUG 0
bool initialised = false;
bool connected = false;
char address[25];

struct UPLOAD_FILES 
{
	char* local_filename;
	char* remote_filename;
};

struct UPLOAD_FILES base_filenames[] = {
		{"esp8266/init.lua","init.lua"},
		{"esp8266/HTTPSE~1.lua","httpserver.lua"},
		{"esp8266/HTTPSE~2.lua","httpserver-error.lua"},
		{"esp8266/HTTPSE~3.lua","httpserver-request.lua"},
		{"esp8266/HTTPSE~4.lua","httpserver-static.lua"},
		{"esp8266/b64.lua","b64.lua"},
};

struct UPLOAD_FILES html_filenames[]  = {
		{"esp8266/INDEX~1.HTM","http/index.html"},
		{"esp8266/index.css","http/index.css"},
		{"esp8266/index.lua","http/index.lua"},
};

static float manual_feedrate[] = MANUAL_FEEDRATE;

//////////////////////////////////////////////////////////////////////
/// Init stuff 
////////////////////////////////////////////////////////////////////

void init_esp8266()
{
	// init wifi 
	wifi.begin(WIFI_BAUDRATE);
	wifi.setTimeout(TIMEOUT); 

	clearResults();
	
    esp8266_reset();
}

void check_upload_esp8266(int type)
{
	if (card.cardOK)
	{
		MYSERIAL.println(F("SD Card found"));
		bool uploaded = false;
		
		// all or base files
		if(type == 0 || type == 1)
		{
			for(int i=0; i < sizeof(base_filenames)/sizeof(base_filenames[0]); i++)
			{
				card.openFile(base_filenames[i].local_filename,true);
				if(card.isFileOpen())
				{
					// upload
					upload_file_esp8266(base_filenames[i].remote_filename);
					card.closefile();
					uploaded = true;
				}
			}
		}
		
		//all or html files
		if(type == 0 || type == 2)
		{
			for(int i=0; i < sizeof(html_filenames)/sizeof(html_filenames[0]); i++)
			{
				card.openFile(html_filenames[i].local_filename,true);
				if(card.isFileOpen())
				{
					// upload
					upload_file_esp8266(html_filenames[i].remote_filename);
					card.closefile();
					uploaded = true;
				}
			}
		}

		
		// if we uploaded something, we need to restart
		if(uploaded)
		{
			esp8266_reset();
		}		
		
	}
	else
	{
		MYSERIAL.println(F("No SD Card found!"));
	}
}

void upload_file_esp8266(char* remote_file)
{
	MYSERIAL.print("uploading: ");
	MYSERIAL.println(remote_file);
	char line_buffer[250];
	char my_char;
	//open file
	clearResults();
	wifi.print(F("file.open(\""));
	wifi.print(remote_file);
	wifi.println(F("\",\"w\")"));
	searchResults("\n>",1000,DEBUG);
	
	while(!card.eof())
	{
	   //get a line of chars
	   int linepos =0;
	   while(1)
	   {
	      
	   	  if(linepos > 240)
		  {
			MYSERIAL.print("Too long line detected, the following file might be corrupted: ");
			MYSERIAL.println(remote_file);
			line_buffer[linepos] =0;
			break;
		  }
		  
		  int16_t n=card.get();
		  my_char = (char)n;
		  
		  if(my_char== '\n' || my_char=='\r' || card.eof())
		  {
			line_buffer[linepos] =0;
			break;
		  }
		  // store char
		  line_buffer[linepos] = my_char;
		  //go to next
		  linepos++;
	   } 
	   
	   if(linepos > 0)
	   {	
			//build line
			String line = "file.writeline([[";
			line.concat(line_buffer);
			line.concat("]])");
			
			//write line to ESP8266
			clearResults();
			wifi.println(line);
			searchResults((char*)line.c_str(),1000,DEBUG);
			searchResults("\n>",1000,DEBUG);		
		}
	}

	//close file 
	clearResults();
	wifi.println(F("file.close()"));
	searchResults("\n>",1000,DEBUG);		
}


void esp8266_reset()
{
	wifi.println(F("node.restart()"));
	if(!searchResults("NodeMCU",5000,DEBUG))
	{
		initialised = false;
		MYSERIAL.println(F("Failed to restart wifi"));
		return;
	}
	clearResults();
	// set SSID / pwd
	wifi.print(F("wifi.sta.config(\""));
	wifi.print(ESP8266_SSID);
	wifi.print(F("\",\""));
	wifi.print(ESP8266_PWD);
	wifi.println("\")");

	if(!searchResults("nodemcu-httpserver running at ",10000,DEBUG))
	{
		initialised = false;
		MYSERIAL.println(F("Failed to start webserver"));
		return;
	}
	wifi.readBytesUntil(13, address, 25);

	clearResults();
	MYSERIAL.print(F("Webservice ready at: "));
	MYSERIAL.println(address);
}

/////////////////////////////////////////////////////////////////////////////
// Handle connections, incomming data and more
/////////////////////////////////////////////////////////////////////////////

void handle_esp8266()
{
	if(wifi.available())
	{
		//process commands
		
		MYSERIAL.write(wifi.read());
	}
	
}


///////////////////////////////////////////////////////////////////////////////
// Helper functions
//////////////////////////////////////////////////////////////////////////////

bool searchResults(char *target, long timeout, int dbg)
{
  int c;
  int index = 0;
  int targetLength = strlen(target);
  int count = 0;
  char _data[255];
  
  memset(_data, 0, 255);

  long _startMillis = millis();
  do {
    c = wifi.read();
    
    if (c >= 0) {

      if (dbg > 0) {
        if (count >= 254) {
          debug(_data);
          memset(_data, 0, 255);
          count = 0;
        }
        _data[count] = c;
        count++;
      }

      if (c != target[index])
        index = 0;
        
      if (c == target[index]){
        if(++index >= targetLength)
		{
          if (dbg > 1)  debug(_data);
          return true;
        }
      }
    }
	
	//manage other things whiel waiting
    manage_heater();
    manage_inactivity();
    lcd_update();
  } while(millis() - _startMillis < timeout);

  if (dbg > 0) {
    if (_data[0] == 0) {
      debug("Failed: No data");
    } else {
      debug("Failed");
      debug(_data);
    }
  }
  return false;
}


void clearResults() 
{
   while(wifi.available() > 0) { wifi.read(); }
}

void debug(char *msg) 
{
  if (DEBUG > 0) 
  {
    MYSERIAL.println(msg);
  } 
}

#endif
