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
#define DEBUG 0

#define STORAGE_SIZE 50
char address[STORAGE_SIZE] = {0};
char mode[STORAGE_SIZE] = {0};
char ssid[STORAGE_SIZE] = {0};

char new_address[STORAGE_SIZE] = {0};
char new_mode[STORAGE_SIZE] = {0};
char new_ssid[STORAGE_SIZE] = {0};
char new_pwd[STORAGE_SIZE] = {0};
char new_sec[STORAGE_SIZE] = {0};

#define CMD_LEN 250
char cmd_buf[CMD_LEN];
int cmd_pos=0;

static float manual_feedrate[] = MANUAL_FEEDRATE;

extern char* lcd_status_message;
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
		esp8266_process_cmd(cmd_buf,cmd_pos);
		//reset command
		cmd_pos=0;
	}
}

void esp8266_process_cmd(char* cmd,int cmd_pos)
{	
	//state commands
	if(strncmp(cmd,"IP",2)==0)
	{
		//store IP
		cmd[cmd_pos-1] = 0;
		snprintf(address,STORAGE_SIZE,"%s",cmd+3);
	}
	else if(strncmp(cmd,"MODE",4)==0)
	{
		//store mode
		cmd[cmd_pos-1] = 0;
		snprintf(mode,STORAGE_SIZE,"MODE:%s",cmd+5);
	}
	else if(strncmp(cmd,"SSID",4)==0)
	{
		//store ssid
		cmd[cmd_pos-1] = 0;
		snprintf(ssid,STORAGE_SIZE,"SSID:%s",cmd+5);
	}
	//STATUS request commands
	else if(strncmp(cmd,"NETWORK",7)==0)
	{
		wifi.print("SEC ");
		wifi.println(new_sec);
		new_sec[0] = 0;
		searchResults("ok\r\n",100,DEBUG);
		wifi.print("MODE ");
		wifi.println(new_mode);
		new_mode[0] = 0;
		searchResults("ok\r\n",100,DEBUG);
		wifi.print("SSID ");
		wifi.println(new_ssid);
		new_ssid[0] = 0;
		searchResults("ok\r\n",100,DEBUG);
		wifi.print("PWD ");
		wifi.println(new_pwd);
		new_pwd[0]=0;
	}
	else if(strncmp(cmd,"TEMP",4)==0)
	{
		String info = "TEMP ";
		info += degHotend(0);
		info += " ";
		info += degTargetHotend(0);
		info += " ";
		#if EXTRUDERS > 1
		info += degHotend(1);
		info += " ";
		info += degTargetHotend(1);
		info += " ";
		#else
		info += "-- -- ";
		#endif
		info +=degBed();
		info += " ";
		info += degTargetBed();
		info += " ";
					
		//send string
		wifi.println(info.c_str());			
	}
	else if(strncmp(cmd,"POS",3)==0)
	{
		String info = "POS ";
		info += current_position[X_AXIS];
		info += " ";
		info += current_position[Y_AXIS];
		info += " ";
		info += current_position[Z_AXIS];
		info += " ";
		//send string
		MYSERIAL.println(info.c_str());
		wifi.println(info.c_str());
	}
	else if(strncmp(cmd,"SD",2)==0)
	{
		String info = "SD ";
		//is a file selected
		if (card.isFileOpen()) info += "yes "; 
		else info += "no ";
		// are we currently printing (with percent)
		if (card.sdprinting) info += card.percentDone();
		else info += "---";
						
		info += " ";
		//current elapsed print time
		if(starttime != 0)
		{
			uint16_t time = millis()/60000 - starttime/60000;
			if((time/60) < 10) info+="0";
			info += time/60;
			info += ':';
			if((time%60) <10) info+="0";
			info += time%60;
			info += " ";
		}else{
			info += "--:-- ";
		}
		//send string
		wifi.println(info);
	}
	//move commands
	else if(strncmp(cmd,"MOVE",4)==0)
	{
		//split string
		double data[5];
		int index =0;
		char* ptr = strtok(cmd+5, " ");
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
	//SETTEMP command
	else if(strncmp(cmd,"SETTEMP",7)==0)
	{
		//split string
		int data[2];
		int index;
		char* ptr = strtok(cmd+7, " ");
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
	else if(strncmp(cmd,"HOME",4)==0)
	{
		int axis = atoi(cmd+5);
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
	// UPLOAD
	else if(strncmp(cmd,"UPLOAD",6)==0)
	{	
		MYSERIAL.print("File upload: ");
	
		//create fitting short filename
		String filename = createShortFilename(cmd+7);
		MYSERIAL.println(filename);
		
		//open file
		card.openFile((char*)filename.c_str(),false,true);
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
			card.write(linebuf);
			wifi.println("ok");
			
			//manage other things as this can take loong
			manage_heater();
			manage_inactivity();
			lcd_update();
		}

	}
	// LIST DIR
	else if(strncmp(cmd,"LIST",4)==0)
	{	
		if (card.cardOK)
		{
			wifi.println("{ \"SD\":\"ok\",");
		}
		else  
		{
			wifi.println("{ \"SD\":\"no SD card\",");
		}
		
		wifi.println("\"Files\":[");
		//TODO subdirs ? 
		uint16_t fileCnt = card.getnrfilenames();
		for(uint16_t i=0;i<fileCnt;i++)
		{
			card.getfilename(i);
			
			//skip dirs
			if (card.filenameIsDir) continue;
			 
			wifi.print("\"");
			wifi.print(card.filename);
			if(i <(fileCnt-1)) wifi.println("\",");
			else  wifi.println("\"");
		}
		wifi.println("]}");
		wifi.println("END");			
	}
	//DELETE file
	else if(strncmp(cmd,"DELETE",6)==0)
	{	
		cmd[cmd_pos-1] = 0;
		card.removeFile(cmd+7);
		wifi.println("ok");
	}
	// Print command
	else if(strncmp(cmd,"PRINT",5)==0)
	{	
		cmd[cmd_pos-1] = 0;
		char cmd2[30];
		char* c;
		sprintf_P(cmd2, PSTR("M23 %s"), cmd+6);
		for(c = &cmd2[4]; *c; c++)  *c = tolower(*c);
		enquecommand(cmd2);
		enquecommand_P(PSTR("M24"));
		wifi.println("ok");
	}
	// Pause command
	else if(strncmp(cmd,"PAUSE",5)==0)
	{
		card.pauseSDPrint();
		wifi.println("ok");
	}
	else if(strncmp(cmd,"RESUME",5)==0)
	{
		card.startFileprint();
		wifi.println("ok");
	}
	else if(strncmp(cmd,"STOP",4)==0)
	{
	    card.sdprinting = false;
		card.closefile();
		quickStop();
		clearbuffer();
    
		if(SD_FINISHED_STEPPERRELEASE)
		{
			enquecommand_P(PSTR(SD_FINISHED_RELEASECOMMAND));
		}	
		autotempShutdown();
    
		cancel_heatup = true;
		wifi.println("ok");
	}
	// UNKNOWN Command
	else
	{
		MYSERIAL.print("Unknown cmd: ");
		MYSERIAL.println(cmd);
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
				if(strncmp(line_buffer,"SSID",4) ==0)
				{
					strcpy(new_ssid,line_buffer+5);
				}
				else if(strncmp(line_buffer,"MODE",4) ==0)
				{
					strcpy(new_mode,line_buffer+5);
				}
				else if(strncmp(line_buffer,"PWD",3) ==0)
				{
					strcpy(new_pwd,line_buffer+4);
				}
				else if(strncmp(line_buffer,"SEC",3) ==0)
				{
					strcpy(new_sec,line_buffer+4);
				}
			}			
		}
	}	
}


///////////////////////////////////////////////////////////////////////////////
// Helper functions
//////////////////////////////////////////////////////////////////////////////

String createShortFilename(char* name)
{
	String filename = name;
	String fileend = "";
	String basename = "";
	
	int dotpos = filename.lastIndexOf(".");
	// file extension
	if(dotpos != -1) fileend = filename.substring(dotpos+1);
	else fileend = "non";
	if(fileend.length() > 3) fileend = fileend.substring(0,3);
	
	// basename
	if(dotpos != -1) basename= filename.substring(0,dotpos);
	else basename = filename;
	
	//try without any counters
	if(basename.length() > 8) 
	{
		basename = basename.substring(0,8);
	}
	filename = basename;
	filename.concat(".");
	filename.concat(fileend);
	
	//check if file exists
	card.openFile((char*)filename.c_str(),true);
	if(!card.isFileOpen()) return filename;
	card.closefile();
	
	//try with counter
	int counter =1;
	while(1)
	{
		if(counter < 10 && basename.length() > 6) basename = basename.substring(0,6);
		if((counter > 10 && counter < 100) && basename.length() > 5 ) basename = basename.substring(0,5);
		if((counter > 100 && counter < 1000) && basename.length() > 4 ) basename = basename.substring(0,4);
		
		//create filename
		filename = basename;
		filename.concat("~");
		filename.concat(counter);
		filename.concat(".");
		filename.concat(fileend);
		
		//check if file exists
		card.openFile((char*)filename.c_str(),true);
		if(!card.isFileOpen()) return filename;
		card.closefile();
		
		counter++;
	}
}


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
	
	//manage other things while waiting
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
