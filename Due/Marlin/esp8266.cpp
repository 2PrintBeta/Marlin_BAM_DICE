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

#define DIN A3
#define DOUT A4

#define wifi Serial1
#define TIMEOUT 5000
#define WIFI_BAUDRATE 1000000
#define DEBUG 2

#define STORAGE_SIZE 50
char address[STORAGE_SIZE] = {0};
char mode[STORAGE_SIZE] = {0};
char ssid[STORAGE_SIZE] = {0};

#define CMD_LEN 250
char cmd_buf[CMD_LEN];
int cmd_pos=0;

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
		
	strcpy(address,"NONE");
	strcpy(mode,"MODE: ");
	strcpy(ssid,"SSID: ");
	
	
	//TODO reset
}

char* esp8266_ip()
{
	return address;
}
char* esp8266_ssid()
{
	return ssid;
}
char* esp8266_mode()
{
	return mode;
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
		if(cmd_pos >= CMD_LEN)
		{
			cmd_pos=0;
			MYSERIAL.println("Error: got a too long command from ESP8266 ! ");
			MYSERIAL.println(cmd_buf);
		}
	}
	
	//process commands
	if(cmd_available)
	{
		//state commands
		if(strncmp(cmd_buf,"IP",2)==0)
		{
			//store IP
			cmd_buf[cmd_pos-1] = 0;
			snprintf(address,STORAGE_SIZE,"%s",cmd_buf+3);
		}
		else if(strncmp(cmd_buf,"MODE",4)==0)
		{
			//store mode
			cmd_buf[cmd_pos-1] = 0;
			snprintf(mode,STORAGE_SIZE,"MODE:%s",cmd_buf+5);
		}
		else if(strncmp(cmd_buf,"SSID",4)==0)
		{
			//store ssid
			cmd_buf[cmd_pos-1] = 0;
			snprintf(ssid,STORAGE_SIZE,"SSID:%s",cmd_buf+5);
		}
		
		//move commands
		else if(strncmp(cmd_buf,"MOVE",4)==0)
		{
			//split string
			double data[5];
			int index =0;
			char* ptr = strtok(cmd_buf+5, " ");
			while(ptr != NULL) 
			{
				data[index] = strtod(ptr,NULL);
				index++;
				// naechsten Abschnitt erstellen
				ptr = strtok(NULL, " ");
			}
			//execute move
			ESP8266_move(data[0],data[1],data[2],data[3],data[4]);
			wifi.println("ok");
		}
		// GETTEMP command
		else if(strncmp(cmd_buf,"GETTEMP",7)==0)
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
		else if(strncmp(cmd_buf,"SETTEMP",7)==0)
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
			wifi.println("ok");
		}
		//HOME command
		else if(strncmp(cmd_buf,"HOME",4)==0)
		{
			int axis = atoi(cmd_buf+5);
			switch(axis)
			{
				case 0:
					enquecommand_P(PSTR("G28"));
					break;
				case 1:
					enquecommand_P(PSTR("G28 X"));
					break;
				case 2:
					enquecommand_P(PSTR("G28 Y"));
					break;
				case 3:
					enquecommand_P(PSTR("G28 Z"));
					break;
			}
			wifi.println("ok");
		}
		else if(strncmp(cmd_buf,"UPLOAD",6)==0)
		{	
			MYSERIAL.print("File upload: ");
			MYSERIAL.println(cmd_buf+7);
			//open file
			//card.openFile(cmd_buf+7,false,true);
			card.openFile("test.txt",false,true);
			if(card.isFileOpen()) wifi.println("ok");
			else  wifi.println("Error file open");
			
			char linebuf[100];
			while(1)
			{
				// get a line of data
				int num = wifi.readBytesUntil('\0',linebuf,100);
				if( num == 0)
				{
					MYSERIAL.println("Timeout ! ");
					card.closefile();
					break;
				}
				linebuf[num] = 0;
				if(strncmp(linebuf,"ENDUPLOAD",9)==0)
				{
					card.closefile();
					wifi.println("ok");
					MYSERIAL.print("File upload ended");
					break;
				}
				//MYSERIAL.print(linebuf);
				card.write(linebuf);
				wifi.println("ok");
				//MYSERIAL.print("ok");
			}

		}
		else
		{
			MYSERIAL.println(cmd_buf);
		}
		//reset command
		cmd_pos=0;
	}
}

void esp8266_load_cfg()
{
	card.openFile("wifi.cfg",true);
	if(card.isFileOpen())
	{
		char line_buffer[60];
		while(!card.eof())
		{
			  //get a line of chars
			int linepos =0;
			while(1)
			{
				if(linepos > 60)
				{
					MYSERIAL.print("Too long line detected in wifi.config"); 
					line_buffer[linepos] =0;
					break;
				}
				
				int16_t n=card.get();
				char my_char = (char)n;
		  
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
			
			// if we have a line, send to wifi module
			if(linepos > 0)
			{
				wifi.println(line_buffer);
				//wait for a response
				if(!searchResults("ok",1000,DEBUG))
				{
					MYSERIAL.println("No answer from esp8266");
					break;
				}
			}			
		}
	}	
}

///////////////////////////////////////////////////////////////////////////////
// Helper functions
//////////////////////////////////////////////////////////////////////////////

void ESP8266_move(double x, double y, double z, double e,int f)
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
		current_position[Y_AXIS] += y;
		if (min_software_endstops && current_position[Y_AXIS] < Y_MIN_POS)
			current_position[Y_AXIS] = Y_MIN_POS;
		if (max_software_endstops && current_position[Y_AXIS] > Y_MAX_POS)
			current_position[Y_AXIS] = Y_MAX_POS;	
	}
	// modify z
    if(z!=0)
	{
		current_position[Z_AXIS] += z;
		if (min_software_endstops && current_position[Z_AXIS] < Z_MIN_POS)
			current_position[Z_AXIS] = Z_MIN_POS;
		if (max_software_endstops && current_position[Z_AXIS] > Z_MAX_POS)
			current_position[Z_AXIS] = Z_MAX_POS;	
    }
	// modify e
    if(e!=0)
	{
		current_position[E_AXIS] += e;
    }
	#ifdef DELTA
    calculate_delta(current_position);
    plan_buffer_line(current_position[X_AXIS], current_position[Y_AXIS], current_position[Z_AXIS], current_position[E_AXIS],f*feedmultiply/60/100.0, active_extruder);
    #else
    plan_buffer_line(current_position[X_AXIS], current_position[Y_AXIS], current_position[Z_AXIS], current_position[E_AXIS], f*feedmultiply/60/100.0, active_extruder);
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
