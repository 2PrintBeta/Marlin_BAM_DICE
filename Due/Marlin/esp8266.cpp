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
#define DEBUG 2
bool initialised = false;
char address[25];

#define CMD_LEN 250
char cmd_buf[CMD_LEN];
int cmd_pos=0;

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
			String line = "file.writeline([==[";
			line.concat(line_buffer);
			line.concat("]==])");
			
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
	
	initialised = true;
}

/////////////////////////////////////////////////////////////////////////////
// Handle connections, incomming data and more
/////////////////////////////////////////////////////////////////////////////

void handle_esp8266()
{
	bool cmd_available = false;
	while(wifi.available())
	{
		// get char
		cmd_buf[cmd_pos] = wifi.read();
		
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
			MYSERIAL.println("Error: got a too long command from ESP8266 ! ");
		}
	}
	
	//process commands
	if(cmd_available)
	{
		//move commands
		if(strncmp(cmd_buf,"MOVE",4)==0)
		{
			//split string
			double coords[3];
			int index =0;
			char* ptr = strtok(cmd_buf+5, " ");
			while(ptr != NULL) 
			{
				coords[index] = strtod(ptr,NULL);
				index++;
				// naechsten Abschnitt erstellen
				ptr = strtok(NULL, " ");
			}
			//execute move
			ESP8266_move(coords[0],coords[1],coords[2]);

		}
		// GETTEMP command
		if(strncmp(cmd_buf,"GETTEMP",7)==0)
		{
			String response = "{\"Temp1\":\"";
			int tCurr=int(degHotend(0) + 0.5);
			int tTarget=int(degTargetHotend(0) + 0.5);
			response.concat(tCurr);
			response.concat("\",\"Temp1Target\":\"");
			response.concat(tTarget);
			response.concat("\",\"Temp2\":\"");
			#if EXTRUDERS > 1
				tCurr = int(degHotend(1) + 0.5);
				tTarget = int(degTargetHotend(1) + 0.5);
				response.concat(tCurr);
				response.concat("\",\"Temp2Target\":\"");
				response.concat(tTarget);
			#else
				response.concat("--");
				response.concat("\",\"Temp2Target\":\"");
				response.concat("--");
			#endif
			response.concat("\",\"Bed\":\"");
			tCurr=int(degBed() + 0.5);
			tTarget=int(degTargetBed() + 0.5);
			response.concat(tCurr);
			response.concat("\",\"BedTarget\":\"");
			response.concat(tTarget);
			response.concat("\"}");
			
			wifi.println(response);
		}
		//SETTEMP command
		if(strncmp(cmd_buf,"SETTEMP",7)==0)
		{
			//split string
			int data[2];
			int index;
			char* ptr = strtok(cmd_buf+7, " ");
			while(ptr != NULL) 
			{
				data[index] = atoi(ptr);
				index++;
				// naechsten Abschnitt erstellen
				ptr = strtok(NULL, " ");
			}
			//set values
			if(data[0] == 1)
			{
				if(data[1] > HEATER_0_MAXTEMP - 15) data[1] =  HEATER_0_MAXTEMP - 15;
				target_temperature[0] = data[1];
			}
			#if EXTRUDERS > 1
			if(data[0] == 2)
			{
				if(data[1] > HEATER_1_MAXTEMP - 15) data[1] =  HEATER_1_MAXTEMP - 15;
				target_temperature[1] = data[1];
			}
			#endif
			if(data[0] == 3)
			{
				if(data[1] > BED_MAXTEMP - 15) data[1] =  BED_MAXTEMP - 15;
				target_temperature_bed = data[1];
			}
		}
		
		//reset command
		cmd_pos=0;
	}
}

///////////////////////////////////////////////////////////////////////////////
// Helper functions
//////////////////////////////////////////////////////////////////////////////

void ESP8266_move(int x, int y, int z)
{
	// modify x
	if(x != 0)
	{
		current_position[X_AXIS] += x;
		if (min_software_endstops && current_position[X_AXIS] < X_MIN_POS)
			current_position[X_AXIS] = X_MIN_POS;
		if (max_software_endstops && current_position[X_AXIS] > X_MAX_POS)
			current_position[X_AXIS] = X_MAX_POS;
	}
	
	// modify y
    if(y!=0)
	{	
		current_position[X_AXIS] += y;
		if (min_software_endstops && current_position[X_AXIS] < X_MIN_POS)
			current_position[X_AXIS] = X_MIN_POS;
		if (max_software_endstops && current_position[X_AXIS] > X_MAX_POS)
			current_position[X_AXIS] = X_MAX_POS;	
	}
	// modify z
    if(z!=0)
	{
		current_position[X_AXIS] += z;
		if (min_software_endstops && current_position[X_AXIS] < X_MIN_POS)
			current_position[X_AXIS] = X_MIN_POS;
		if (max_software_endstops && current_position[X_AXIS] > X_MAX_POS)
			current_position[X_AXIS] = X_MAX_POS;	
    }
	
	#ifdef DELTA
    calculate_delta(current_position);
    plan_buffer_line(delta[X_AXIS], delta[Y_AXIS], delta[Z_AXIS], current_position[E_AXIS], manual_feedrate[X_AXIS]/60, active_extruder);
    #else
    plan_buffer_line(current_position[X_AXIS], current_position[Y_AXIS], current_position[Z_AXIS], current_position[E_AXIS], manual_feedrate[X_AXIS]/60, active_extruder);
    #endif
}

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
