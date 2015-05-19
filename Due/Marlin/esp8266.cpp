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
bool network_changed = false;

static float manual_feedrate[] = MANUAL_FEEDRATE;

extern char* lcd_status_message;


///////////////////////////////////////////////////////////////////
// COm Protocol
///////////////////////////////////////////////////////////////////
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
	eGcode,
	
	eGetNetworkSSID,
	eGetNetworkPWD,
	eGetNetworkMode,
	eGetNetworkSec,
	
	eGetNumSDEntries,
	eGetSDEntry,
	
	eSetNetworkSSID,  //this commands return the current state
	eSetNetworkMode,  //this commands return the current state
	eSetNetworkIP,   //this commands return the current state
	
	// answers
	eOk,
	eError,
	
	//used for debug
	eDebug,
};

struct S_STATE
{
	char networkChanged;
	char fileOpen;
	char sdPlugged;
	char percentage;
	unsigned long printtime;
	float temp1;
	float temp1Target;
	float temp2;
	float temp2Target;
	float tempBed;
	float tempBedTarget;
	float x;
	float y;
	float z;
	char fanSpeed;
};

struct comm_package
{
	ESP_CMDs cmd;
	unsigned char length;
	unsigned char data[61];
	unsigned char checksum;  //xor sum from cmd till data end
};

//buffers for the commands/answers
comm_package esp_cmd;
int esp_data_index=0;
comm_package esp_answer;

enum ESP_COMM_STATE
{
	eIdle,
	eHeader,
	eData,
	eChecksum,
};

ESP_COMM_STATE comm_state = eIdle;
unsigned long cmd_start_time=0;
#define CMD_TIMEOUT 500
#define CRC_MAGIC 0xE

/////////////////////////////////////////////////////////////////
// helper functions
////////////////////////////////////////////////////////////////
void ESP8266_move(double x, double y, double z, double e,int f);
String createShortFilename(char* name);
void debug(char *msg) ;
void handle_cmd();
unsigned char calc_crc(comm_package* pkt);
void esp_send_2(bool ok, char* data);
void esp_send();
void esp_send_state();
void wifi_write(const uint8_t c);
//////////////////////////////////////////////////////////////////////
/// Init stuff 
////////////////////////////////////////////////////////////////////

void init_esp8266()
{
	// inti variables
	comm_state = eIdle;
	strcpy(address,"NONE");
	strcpy(mode,"MODE: ");
	strcpy(ssid,"SSID: ");

	//setup pins, reset esp
	pinMode(ESP_RESET_PIN,OUTPUT);
	pinMode(ESP_CH_DOWN_PIN,OUTPUT);
	pinMode(ESP_PROG_PIN,OUTPUT);
	
	digitalWrite(ESP_RESET_PIN,HIGH);
	digitalWrite(ESP_CH_DOWN_PIN,HIGH);
	digitalWrite(ESP_PROG_PIN,HIGH);
	
	//reset esp
    digitalWrite(ESP_RESET_PIN,LOW);
    delay(100);
    digitalWrite(ESP_RESET_PIN,HIGH);
	
	// init wifi 
	wifi.begin(WIFI_BAUDRATE);
	wifi.setTimeout(TIMEOUT);
		
	// clear data
	while(wifi.available()) wifi.read();
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
	bool cmd_ready = false;
	//read from wifi serial, aslong as there are chars available
	while(wifi.available())
	{
		unsigned char c = wifi.read();
		switch(comm_state)
		{
			case eIdle:
				esp_cmd.cmd=(ESP_CMDs)c;
				//TODO check for valid command ?
				esp_data_index =0;
				cmd_start_time = millis(); //remember start time for timeouts
				comm_state = eHeader;
				break;
			case eHeader:
				esp_cmd.length = c;
				if(esp_cmd.length > 60) 
				{	
					debug("Invalid Length recieved from esp");
					comm_state = eIdle;
				}
				if(esp_cmd.length > 0) comm_state = eData;
				else comm_state = eChecksum;
				break;
			case eData:
				esp_cmd.data[esp_data_index] = c;
				esp_data_index++;
				if(esp_data_index >= esp_cmd.length) comm_state = eChecksum;
				break;
			case eChecksum:
				esp_cmd.checksum = c;
				comm_state = eIdle;
				cmd_ready = true;
				break;
		}
		// stop reading if we have a full command
		if(cmd_ready) break;
	}

	// we have a cmd in buffer, handle it
	if(cmd_ready)
	{
		handle_cmd();
	}
	
	//check for timeouts
	if(comm_state != eIdle)
	{
		if((millis() -cmd_start_time) > CMD_TIMEOUT)
		{
			debug("esp_cmd timeout");
			comm_state = eIdle;
			//close file
			if(card.isFileOpen()) card.closefile();
		}
	}
}

void handle_cmd()
{
#if DEBUG > 0
	MYSERIAL.print("new cmd: ");
	MYSERIAL.print((int)esp_cmd.cmd);
	MYSERIAL.print(" ");
	MYSERIAL.print((int)esp_cmd.length);
	MYSERIAL.print(" ");
	esp_cmd.data[esp_cmd.length] =0;
	MYSERIAL.print((char*)esp_cmd.data);
	MYSERIAL.print(" ");
	MYSERIAL.println((int)esp_cmd.checksum);	
#endif
	//check checksum
	unsigned char checksum = calc_crc(&esp_cmd);
	if(esp_cmd.checksum != checksum)
	{
		debug("Invalid CRC recieved from ESP");
		return;
	}
	
	//handle commands
	switch(esp_cmd.cmd)
	{
		case eDebug:
		{
			esp_cmd.data[esp_cmd.length] =0;
			#if DEBUG > 0
			MYSERIAL.print("dbg: ");
			MYSERIAL.println((char*)esp_cmd.data);
			#endif
			//no send !
			break;
		}
		case eSync:
		{
			esp_send_2(true,"");
			break;
		}
		case eSetTemp:
		{
			char heater = -1;
			short temp = -1;
			memcpy(&heater,esp_cmd.data,1);
			memcpy(&temp,esp_cmd.data+1,2);

			if(heater == 1)
			{
				if(temp > HEATER_0_MAXTEMP - 15) temp=  HEATER_0_MAXTEMP - 15;
				target_temperature[0] = temp;
			}
			#if EXTRUDERS > 1
			else if(heater == 2)
			{
				if(temp > HEATER_1_MAXTEMP - 15) temp =  HEATER_1_MAXTEMP - 15;
				target_temperature[1] = dtemp;
			}
			#endif
			else if(heater == 3)
			{
				if(temp > BED_MAXTEMP - 15) temp =  BED_MAXTEMP - 15;
				target_temperature_bed = temp;
			}
			else
			{
				//error
				esp_send_2(false,"Invalid parameters");
				break;
			}

			//send answer
			esp_send_2(true,"");
			break;
		}
		case eSetFan:
		{
			char speed = 0;
			memcpy(&speed,esp_cmd.data,1);
			
			fanSpeed = speed;
			//send answer
			esp_send_2(true,"");
			break;
		}
		case eMove:
		{
			//do not move while homing
			if(homing_in_progress) return;
			
			float x,y,z,e,f;
			memcpy(&x,esp_cmd.data,4);
			memcpy(&y,esp_cmd.data+4,4);
			memcpy(&z,esp_cmd.data+8,4);
			memcpy(&e,esp_cmd.data+12,4);
			memcpy(&f,esp_cmd.data+16,4);
						
			//execute move
			ESP8266_move(x,y,z,e,f);
			
			//send answer
			esp_send_2(true,"");
			break;
		}
		case eHome:
		{
			char axis = esp_cmd.data[0];
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
			//send answer
			esp_send_2(true,"");
			break;
		}
		case ePrint:
		{
			// get data
			char filename[61];
			memcpy(filename,esp_cmd.data,esp_cmd.length);
			filename[esp_cmd.length] = 0;
		
			//create a gcode
			char cmd[65];
			char* c;
			sprintf_P(cmd, PSTR("M23 %s"), filename);
			for(c = &cmd[4]; *c; c++)  *c = tolower(*c);
			enquecommand(cmd);
			enquecommand_P(PSTR("M24"));
			
			//send answer
			esp_send_2(true,"");
			break;
		}
		case ePause:
		{
			card.pauseSDPrint();
			//send answer
			esp_send_2(true,"");
			break;
		}
		case eResume:
		{
			card.startFileprint();
			//send answer
			esp_send_2(true,"");
			break;
		}
		case eStop:
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
			starttime=0;
			//send answer
			esp_send_2(true,"");
			break;
		}
		case eDelete:
		{
			//check if we have a SD card
			if(!card.cardOK)
			{
				esp_send_2(false,"No SD card");
				break;
			}
			//get filename
			char filename[61];
			memcpy(filename,esp_cmd.data,esp_cmd.length);
			filename[esp_cmd.length] = 0;
			if(card.removeFile(filename))
			{
				esp_send_2(true,"");
			}
			else
			{
				esp_send_2(true,"Could not delete File");
			}
			break;
		}
		case eOpenFile:
		{
			debug("File upload: ");
			//check if we have a SD card
			if(!card.cardOK)
			{
				esp_send_2(false,"No SD card");
				break;
			}
			//check if we already have a file open
			if(card.isFileOpen())
			{
				esp_send_2(false,"Upload in progress");
				break;
			}
			
			// get data
			char filename[61];
			memcpy(filename,esp_cmd.data,esp_cmd.length);
			filename[esp_cmd.length] = 0;
						
			//create fitting short filename
			String short_file = createShortFilename(filename);
		
			//open file
			card.openFile((char*)short_file.c_str(),false,true);
			if(card.isFileOpen())
			{
				esp_send_2(true,"");
			}
			else 
			{
				esp_send_2(false,"File open failed");
			}
			break;
		}
		case eCloseFile:
		{
			//check if we have a SD card
			if(!card.cardOK)
			{
				esp_send_2(false,"No SD card");
				break;
			}
			card.closefile();
			esp_send_2(true,"");
			break;
		}
		case eFileData:
		{
			//check if we have a SD card
			if(!card.cardOK)
			{
				esp_send_2(false,"No SD card");
				break;
			}
			//check if file is open
			if(!card.isFileOpen())
			{
				esp_send_2(false,"No file open");
				break;
			}
			
			//write data
			if(card.write((char*)esp_cmd.data,esp_cmd.length)) esp_send_2(true,"");
			else esp_send_2(false,"write failed");
					
			break;
		}
		case eGcode:
		{
			esp_cmd.data[esp_cmd.length] =0;
			enquecommand((char*)esp_cmd.data);
			esp_send_2(true,"");
			break;
		}
		case eGetNetworkSSID:
		{
			esp_answer.cmd = eOk;
			esp_answer.length = strlen(new_ssid);
			memcpy(esp_answer.data,new_ssid,esp_answer.length);
			
			esp_send();
			break;
		}
		case eGetNetworkPWD:
		{
			esp_answer.cmd = eOk;
			esp_answer.length = strlen(new_pwd);
			memcpy(esp_answer.data,new_pwd,esp_answer.length);
			
			esp_send();
			break;
		}
		case eGetNetworkMode:
		{
			esp_answer.cmd = eOk;
			esp_answer.length = strlen(new_mode);
			memcpy(esp_answer.data,new_mode,esp_answer.length);
			
			esp_send();
			break;
		}
		case eGetNetworkSec:
		{
			esp_answer.cmd = eOk;
			esp_answer.length = strlen(new_sec);
			memcpy(esp_answer.data,new_sec,esp_answer.length);
			
			esp_send();
			break;
		}
		case eSetNetworkSSID:
		{
			esp_cmd.data[esp_cmd.length] = 0;
			snprintf(ssid,STORAGE_SIZE,"SSID: %s",esp_cmd.data);
			//send state
			esp_send_state();
			break;
		}
		case eSetNetworkMode:
		{
			esp_cmd.data[esp_cmd.length] = 0;
			snprintf(mode,STORAGE_SIZE,"MODE: %s",esp_cmd.data);
			//send state
			esp_send_state();
			break;
		}
		case eSetNetworkIP:
		{
			esp_cmd.data[esp_cmd.length] = 0;
			snprintf(address,STORAGE_SIZE,"%s",esp_cmd.data);
			//send state
			esp_send_state();
			break;
		}
		case eGetNumSDEntries:
		{
			if(!card.cardOK)
			{
				esp_send_2(false,"No SD card");
				break;
			}
			
			uint16_t fileCnt = card.getnrfilenames();
			esp_answer.cmd = eOk;
			esp_answer.length =2;
			memcpy(esp_answer.data,&fileCnt,2);
			
			esp_send();
			break;
		}
		case eGetSDEntry:
		{
			if(!card.cardOK)
			{
				esp_send_2(false,"No SD card");
				break;
			}
			uint16_t file_index = 0;
			memcpy(&file_index,esp_cmd.data,2);
			
			card.getfilename(file_index);
			esp_answer.cmd = eOk;
			esp_answer.length = strlen(card.filename);
			memcpy(esp_answer.data,card.filename,esp_answer.length);
			esp_send();

			break;
		}
		default:
			debug("Unknown command recieved from ESP");
			break;
	}
}

unsigned char calc_crc(comm_package* pkt)
{
	unsigned char checksum = CRC_MAGIC;
	checksum = checksum ^ pkt->cmd;
	checksum = checksum ^ pkt->length;
	
	for(int i=0; i < pkt->length; i++)
	{
		checksum = checksum ^ pkt->data[i];
	}
	return checksum;
}

void esp_send_2(bool ok, char* data)
{
	if(ok) esp_answer.cmd = eOk;
	else esp_answer.cmd = eError;
	
	esp_answer.length = strlen(data);
	memcpy(esp_answer.data,data,esp_answer.length);	
	
	esp_send();
}
void esp_send_state()
{
	esp_answer.cmd = eOk;
	esp_answer.length = sizeof(S_STATE);

	S_STATE* state = (S_STATE*)esp_answer.data;
	// if network changed
	state->networkChanged = network_changed;
	// if a file is opened
	state->fileOpen = card.isFileOpen();
	// print percentage
	if (card.sdprinting) state->percentage =card.percentDone();
	else state->percentage = 255;
	//print time
	if(starttime != 0) state->printtime = millis() - starttime;
	else state->printtime =0;
	//sd card inserted
	state->sdPlugged = card.cardOK;
	//fan speed
	state->fanSpeed = fanSpeed;
	//temperatures
	state->temp1 = degHotend(0);
	state->temp1Target = degTargetHotend(0);
	#if EXTRUDERS > 1
	state->temp2 = degHotend(1);
	state->temp2Target = degTargetHotend(1);
	#else
	state->temp2 = -100.0;
	state->temp2Target = -100.0;
	#endif
	state->tempBed = degBed();
	state->tempBedTarget = degTargetBed();
	// position
	state->x = current_position[X_AXIS];
	state->y = current_position[Y_AXIS];
	state->z = current_position[Z_AXIS];
	
	//send
	esp_send();
	network_changed = false;
}

void esp_send()
{
	//calc checksum
	esp_answer.checksum = calc_crc(&esp_answer);
				
	//send packet
	wifi_write(esp_answer.cmd);
	wifi_write(esp_answer.length);
	for(int i=0;i < esp_answer.length; i++)
	{
		wifi_write(esp_answer.data[i]);
	}
	wifi_write(esp_answer.checksum);
}

void wifi_write(const uint8_t c)
{
	//TODO find out why normal, buffered send do not work correctly
	
	//workaround send uart data to ESP8266 directly without buffering
	Uart* _pUart =(Uart*) USART0;
        // Check if the transmitter is ready
  
	while ((_pUart->UART_SR & UART_SR_TXRDY) != UART_SR_TXRDY) 
    {
        
    }

	// Send character
	 _pUart->UART_THR = c;
}

////////////////////////////////////////////////////////////
// Load config file, store in buffers for sending on request
/////////////////////////////////////////////////////////////

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
			
			// if we have a line, store in mem
			if(linepos > 0)
			{
				if(strncmp(line_buffer,"SSID",4) ==0)
				{
					strcpy(new_ssid,line_buffer+5);
					network_changed = true;
				}
				else if(strncmp(line_buffer,"MODE",4) ==0)
				{
					strcpy(new_mode,line_buffer+5);
					network_changed = true;
				}
				else if(strncmp(line_buffer,"PWD",3) ==0)
				{
					strcpy(new_pwd,line_buffer+4);
					network_changed = true;
				}
				else if(strncmp(line_buffer,"SEC",3) ==0)
				{
					strcpy(new_sec,line_buffer+4);
					network_changed = true;
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

void debug(char *msg) 
{
  if (DEBUG > 0) 
  {
    MYSERIAL.println(msg);
  } 
}

#endif
