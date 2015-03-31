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
#include "temperature.h"
#ifdef HAVE_ESP8266

#define wifi Serial1
#define TIMEOUT 5000
#define WIFI_TIMEOUT 0
#define WIFI_BAUDRATE 115200
#define DEBUG 2
bool initialised = false;
bool connected = false;
char _ipaddress[15];
char WIFI_Host[24];
int WIFI_Channel;

// this is used to calculate size in a first run, and send in the second
byte HTML_Send_Mode;
#define HTML_SEND_MODE_PREPARE 0
#define HTML_SEND_MODE_SEND 1
int HTML_Content_Length;
int HTML_Header_Length;
int HTML_Temp_Length;
int HTML_Sent_Length;
int HTML_Send_Length;
#define HTML_CHUNK_LENGTH 1400

//---------------------------------------------------------------------------
// Receive-Buffer
#define RECV_BUFFER_SIZE 100
char RECV_Buffer[RECV_BUFFER_SIZE];


static float manual_feedrate[] = MANUAL_FEEDRATE;

//////////////////////////////////////////////////////////////////////
/// Init stuff 
////////////////////////////////////////////////////////////////////


void init_esp8266()
{
	// init wifi 
	wifi.begin(WIFI_BAUDRATE);
	wifi.setTimeout(TIMEOUT); 
  
	//delay(500);
	clearResults();
  
	// check for presence of wifi module
	wifi.println(F("AT"));
	delay(500);
	if(!searchResults("OK", 1000, DEBUG)) 
	{
		//try to change baudrate
		wifi.end();
		wifi.begin(9600);
		wifi.setTimeout(TIMEOUT); 
  
		//check response
		wifi.println(F("AT"));
		delay(500);
		if(!searchResults("OK", 1000, DEBUG)) 
		{
			initialised = false;
			MYSERIAL.println("Could not connect to ESP8266 with baud9600");
			return;
		}
	
		//try to change baudrate
		wifi.print(F("AT+BAUD="));
		wifi.println(WIFI_BAUDRATE);
		delay(500);
		if(!searchResults("OK", 1000, DEBUG)) 
		{
			initialised = false;
			MYSERIAL.println("Could not change baudrate to 115200");
			return;
		}
		wifi.end();
	
		wifi.begin(WIFI_BAUDRATE);
		wifi.setTimeout(TIMEOUT); 
	
		wifi.println(F("AT"));
		delay(500);
		if(!searchResults("OK", 1000, DEBUG)) 
		{
			initialised = false;
			MYSERIAL.println("Could not connect to ESP8266 after changing baudrate");
			return;
		}
	} 
  
	// reset WiFi module
	wifi.println(F("AT+RST"));
	delay(500);
	if(!searchResults("ready", 6000, DEBUG)) 
	{
		initialised = false;
		MYSERIAL.println("Could not reset ESP8266");
		return;
	}
    delay(500);
  
	// set the connectivity mode 1=sta, 2=ap, 3=sta+ap
	wifi.print(F("AT+CWMODE="));
	wifi.println(ESP8266_MODE);
	
	clearResults();
  
	// set the access point value and connect
	wifi.print(F("AT+CWJAP=\""));
	wifi.print(ESP8266_SSID);
	wifi.print(F("\",\""));
	wifi.print(ESP8266_PWD);
	wifi.println(F("\""));
	delay(100);
	if(!searchResults("OK", 30000, DEBUG)) 
	{
		initialised = false;
		MYSERIAL.println("Could not connect to Access point");
		return;
	}
  
	// enable multi-connection mode
	wifi.println(F("AT+CIPMUX=1"));;
	delay(500);
	if(!searchResults("OK", 1000, DEBUG))
	{
		initialised = false;
		MYSERIAL.println("Could not enable multi connection mode");
		return;
	}
	
	//start the server
	wifi.print(F("AT+CIPSERVER=1,"));
	wifi.println(ESP8266_PORT);
	if(!searchResults("OK", 500, DEBUG))
	{
		initialised = false;
		MYSERIAL.println("Could not start server mode");
		return;
	}
	// set to only one connection
	/*wifi.println(F("AT+CIPMUX=0"));;
	delay(500);
	if(!searchResults("OK", 1000, DEBUG))
	{
		initialised = false;
		MYSERIAL.println("Could not enable multi connection mode");
		return;
	}
  */
	// set timeout
	wifi.print(F("AT+CIPSTO="));
	wifi.println(WIFI_TIMEOUT);
	if(!searchResults("OK", 500, DEBUG)) 
	{
		initialised = false;
		MYSERIAL.println("Could not change server timeout");
		return;
	}	
		
	// get Ip
    if(!getIP())
	{
		initialised = false;
		MYSERIAL.println("Could not get own ip");
		return;
	}	
	MYSERIAL.println(F("Wifi connected"));
	MYSERIAL.println(_ipaddress);
	
}

/////////////////////////////////////////////////////////////////////////////
// Handle connections, incomming data and more
/////////////////////////////////////////////////////////////////////////////

void handle_esp8266()
{
	int WIFI_Packet_Length;
	int i;
	char *buffer_pointer;
	byte len;
	
	 // request: +IPD,ch,len:GET /?LED=xxx&PULS=nnnn ... Host: nnn.nnn.nnn.nnn:nnnn 0x0D ...

	if (wifi.findUntil("+IPD,", "\r")) 
	{	
		// get length
		WIFI_Channel = wifi.parseInt();
		wifi.findUntil(",", "\r");
		WIFI_Packet_Length = wifi.parseInt();
		
		// GET command
		if (wifi.findUntil("GET /", "\r")) 
		{
			wifi.readBytesUntil(13, RECV_Buffer, RECV_BUFFER_SIZE);
			if (WIFI_Packet_Length > 0) 
			{
				// get host
				WIFI_Host[0] = 0;
				if (wifi.find("Host: ")) 
				{
					len = wifi.readBytesUntil(13, WIFI_Host, 23);
					WIFI_Host[len] = 0;
				}
							
				// search for end of request data
				if(!searchResults("OK",1000,DEBUG)) MYSERIAL.println("Could not find OK");
				
				//process request
				buffer_pointer = RECV_Buffer;
				process_get(buffer_pointer);

				clearResults();
				//send html page
				HTML_Page(WIFI_Channel);
				clearResults();
			}
		}

		//clear data in buffers
		clearResults();
	}
	
}

void process_get(char* buffer_pointer)
{
	//  FAN handling 
	if (strncmp(buffer_pointer, "?FAN=", 5) == 0) 
	{
		buffer_pointer += 5;
		if (strncmp(buffer_pointer, "On", 2) == 0) 
		{
			fanSpeed = 255;
		}
		if (strncmp(buffer_pointer, "Off", 3) == 0) 
		{
			fanSpeed =0;
		}
		buffer_pointer += 3;
	}
	// MOVEX Handling
	if (strncmp(buffer_pointer, "?MOVEX=", 7) == 0) 
	{
		buffer_pointer += 7;
		if (strncmp(buffer_pointer, "100", 3) == 0) 
		{
			ESP8266_move(100,0,0);
		}
		else if (strncmp(buffer_pointer, "10", 2) == 0) 
		{
			ESP8266_move(10,0,0);
		}
		else if (strncmp(buffer_pointer, "1", 1) == 0) 
		{
			ESP8266_move(1,0,0);
		}
		else if (strncmp(buffer_pointer, "-100", 4) == 0) 
		{
			ESP8266_move(-100,0,0);
		}
		else if (strncmp(buffer_pointer, "-10", 3) == 0) 
		{
			ESP8266_move(-10,0,0);
		}
		else if (strncmp(buffer_pointer, "-1", 2) == 0) 
		{
			ESP8266_move(-1,0,0);
		}
	}
	// MOVEY Handling
	if (strncmp(buffer_pointer, "?MOVEY=", 7) == 0) 
	{
		buffer_pointer += 7;
		if (strncmp(buffer_pointer, "100", 3) == 0) 
		{
			ESP8266_move(0,100,0);
		}
		else if (strncmp(buffer_pointer, "10", 2) == 0) 
		{
			ESP8266_move(0,10,0);
		}
		else if (strncmp(buffer_pointer, "1", 1) == 0) 
		{
			ESP8266_move(0,1,0);
		}
		else if (strncmp(buffer_pointer, "-100", 4) == 0) 
		{
			ESP8266_move(0,-100,0);
		}
		else if (strncmp(buffer_pointer, "-10", 3) == 0) 
		{
			ESP8266_move(0,-10,0);
		}
		else if (strncmp(buffer_pointer, "-1", 2) == 0) 
		{
			ESP8266_move(0,-1,0);
		}
	}
	// MOVEZ Handling
	if (strncmp(buffer_pointer, "?MOVEZ=", 7) == 0) 
	{
		buffer_pointer += 7;
		if (strncmp(buffer_pointer, "100", 3) == 0) 
		{
			ESP8266_move(0,0,100);
		}
		else if (strncmp(buffer_pointer, "10", 2) == 0) 
		{
			ESP8266_move(0,0,10);
		}
		else if (strncmp(buffer_pointer, "1", 1) == 0) 
		{
			ESP8266_move(0,0,1);
		}
		else if (strncmp(buffer_pointer, "-100", 4) == 0) 
		{
			ESP8266_move(0,0,-100);
		}
		else if (strncmp(buffer_pointer, "-10", 3) == 0) 
		{
			ESP8266_move(0,0,-10);
		}
		else if (strncmp(buffer_pointer, "-1", 2) == 0) 
		{
			ESP8266_move(0,0,-1);
		}
	}
	// STOP Handling
	if (strncmp(buffer_pointer, "?STOP=", 6) == 0) 
	{
		buffer_pointer += 7;
		st_synchronize();
		disable_e0();
		disable_e1();
		disable_e2();
		finishAndDisableSteppers();
	}
	// GCODE Handling
	if (strncmp(buffer_pointer, "?GCODE=", 7) == 0) 
	{
		buffer_pointer += 7;
		
		// replace + with space
		for(int i=0; buffer_pointer[i] != 13; i++)
		{
			if(buffer_pointer[i] == ' ') buffer_pointer[i] = 0;
			if(buffer_pointer[i] == '+') buffer_pointer[i] = ' ';
		}
		enquecommand(buffer_pointer);
	}
				

}

void ESP8266_move(int x, int y, int z)
{
	// modify x
    current_position[X_AXIS] += x;
    if (min_software_endstops && current_position[X_AXIS] < X_MIN_POS)
        current_position[X_AXIS] = X_MIN_POS;
    if (max_software_endstops && current_position[X_AXIS] > X_MAX_POS)
        current_position[X_AXIS] = X_MAX_POS;

	// modify y
    current_position[X_AXIS] += y;
    if (min_software_endstops && current_position[X_AXIS] < X_MIN_POS)
        current_position[X_AXIS] = X_MIN_POS;
    if (max_software_endstops && current_position[X_AXIS] > X_MAX_POS)
        current_position[X_AXIS] = X_MAX_POS;	
		
	// modify z
    current_position[X_AXIS] += z;
    if (min_software_endstops && current_position[X_AXIS] < X_MIN_POS)
        current_position[X_AXIS] = X_MIN_POS;
    if (max_software_endstops && current_position[X_AXIS] > X_MAX_POS)
        current_position[X_AXIS] = X_MAX_POS;	
    
	#ifdef DELTA
    calculate_delta(current_position);
    plan_buffer_line(delta[X_AXIS], delta[Y_AXIS], delta[Z_AXIS], current_position[E_AXIS], manual_feedrate[X_AXIS]/60, active_extruder);
    #else
    plan_buffer_line(current_position[X_AXIS], current_position[Y_AXIS], current_position[Z_AXIS], current_position[E_AXIS], manual_feedrate[X_AXIS]/60, active_extruder);
    #endif
}

////////////////////////////////////////////////////////////////////////////
// Serve HTML Page
///////////////////////////////////////////////////////////////////////////////
void HTML_Page(int WIFI_Channel) 
{
  //calc page length  
  HTML_Send_Mode = HTML_SEND_MODE_PREPARE;
  
  // content length
  HTML_Temp_Length = 0;
  HTML_Make_Content();
  HTML_Content_Length = HTML_Temp_Length;

  // Header length
  HTML_Send_Mode = HTML_SEND_MODE_PREPARE;
  HTML_Temp_Length = 0;
  HTML_Make_Header();
  HTML_Header_Length = HTML_Temp_Length;

  //send header
  HTML_Sent_Length =0;
  HTML_Send_Length = HTML_Header_Length;
  ESP8266_Send_Header(WIFI_Channel,HTML_Header_Length,true);
  HTML_Send_Mode = HTML_SEND_MODE_SEND;
  HTML_Make_Header();
  
  //send content - if length is over HTML_CHUNK_LENGTH, we make seperate transfers
  HTML_Sent_Length =0;
  HTML_Send_Length = HTML_Content_Length;
  if(HTML_Content_Length > HTML_CHUNK_LENGTH) ESP8266_Send_Header(WIFI_Channel,HTML_CHUNK_LENGTH,false);
  else ESP8266_Send_Header(WIFI_Channel,HTML_Content_Length,false);

  HTML_Send_Mode = HTML_SEND_MODE_SEND;
  HTML_Make_Content();
}

void ESP8266_Send_Header(int channel,int length,bool first)
{
  if(!first)
  {
	if(!searchResults("SEND OK",1000,DEBUG)) MYSERIAL.println("Could not find SEND OK");
	delay(50);
  }
  wifi.print(F("AT+CIPSEND="));
  wifi.print(WIFI_Channel);
  wifi.print(F(","));
  wifi.println(length);
  if(!searchResults(">",1000,DEBUG)) MYSERIAL.println("Could not find >");
}

void HTML_Make_Header() 
{
  HTML_Send_PROGMEM(F("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\n"));
  HTML_Send_PROGMEM(F("Content-Length:"));
  HTML_Send_Int(HTML_Content_Length,0);
  HTML_Send_PROGMEM(F("\r\n\r\n"));
}

void HTML_Make_Content()
{
  HTML_Send_PROGMEM(F("<HTML><HEAD> <title>BAM&amp;DICE Marlin Control</title></HEAD>\n"));
  HTML_Send_PROGMEM(F("<BODY bgcolor=\"#37CF2B\" text=\"#000000\">"));

  HTML_Send_PROGMEM(F("<FONT size=\"6\" FACE=\"Verdana\">BAM&amp;DICE Control<BR/></FONT>"));

  HTML_Send_PROGMEM(F("<BR/>\n"));

  //temperature
  HTML_Send_PROGMEM(F("<table border=\"1\">"));
  HTML_Send_PROGMEM(F("<tr>"));
  HTML_Send_PROGMEM(F("<th>Temperature 1</th>"));
#if EXTRUDERS > 1  
  HTML_Send_PROGMEM(F("<th> Temperature 2</th>"));
#endif  
#if TEMP_SENSOR_BED != 0
  HTML_Send_PROGMEM(F("<th> Temperature Bed</th>"));
 #endif
  HTML_Send_PROGMEM(F("</tr><tr><td>"));
  HTML_Send_Int(int(degHotend(0) + 0.5),3);
  HTML_Send_PROGMEM(F(" / "));
  HTML_Send_Int(int(degTargetHotend(0) + 0.5),3);
#if EXTRUDERS > 1    
  HTML_Send_PROGMEM(F("</td> <td>"));
  HTML_Send_Int(int(degHotend(1) + 0.5),3);
  HTML_Send_PROGMEM(F(" / "));
  HTML_Send_Int(int(degTargetHotend(1) + 0.5),3);
#endif  
#if TEMP_SENSOR_BED != 0
  HTML_Send_PROGMEM(F("</td> <td>"));
  HTML_Send_Int(int(degBed() + 0.5),3);
  HTML_Send_PROGMEM(F(" / "));
  HTML_Send_Int(int(degTargetBed() + 0.5),3);
#endif  
  HTML_Send_PROGMEM(F("</td>"));
  HTML_Send_PROGMEM(F("</tr></table>"));
  
  HTML_Send_PROGMEM(F("<BR/>"));
  
  // form - movements
  HTML_Send_PROGMEM(F("<FORM ACTION=\"http://"));
  HTML_Send(WIFI_Host);
  HTML_Send_PROGMEM(F("\">"));
  HTML_Send_PROGMEM(F("<table><tr>"));
  HTML_Send_PROGMEM(F("<td></td><td></td><td></td><td></td><td>"));
  HTML_Send_PROGMEM(F("<BUTTON TYPE=\"SUBMIT\" NAME=\"MOVEY\" VALUE=\"100\" STYLE=\"Width:60\" WIDTH=\"60\">Y+100</BUTTON>"));
  HTML_Send_PROGMEM(F("</td><td></td><td></td><td></td><td></td><td></td>"));
  HTML_Send_PROGMEM(F("</tr><tr>"));
  HTML_Send_PROGMEM(F("<td></td><td></td><td></td><td></td><td>"));
  HTML_Send_PROGMEM(F("<BUTTON TYPE=\"SUBMIT\" NAME=\"MOVEY\" VALUE=\"10\" STYLE=\"Width:60\" WIDTH=\"60\">Y+10</BUTTON>"));
  HTML_Send_PROGMEM(F("</td><td></td><td></td><td></td><td></td><td>"));
  HTML_Send_PROGMEM(F("<BUTTON TYPE=\"SUBMIT\" NAME=\"MOVEZ\" VALUE=\"10\" STYLE=\"Width:60\" WIDTH=\"60\">Z+10</BUTTON>"));
  HTML_Send_PROGMEM(F("</td>"));
  HTML_Send_PROGMEM(F("</tr><tr>"));
  HTML_Send_PROGMEM(F("<td></td><td></td><td></td><td></td><td>"));
  HTML_Send_PROGMEM(F("<BUTTON TYPE=\"SUBMIT\" NAME=\"MOVEY\" VALUE=\"1\" STYLE=\"Width:60\" WIDTH=\"60\" >Y+1</BUTTON>"));
  HTML_Send_PROGMEM(F("</td><td></td><td></td><td></td><td></td><td>"));
  HTML_Send_PROGMEM(F("<BUTTON TYPE=\"SUBMIT\" NAME=\"MOVEZ\" VALUE=\"1\" STYLE=\"Width:60\" WIDTH=\"60\" >Z+1</BUTTON>"));
  HTML_Send_PROGMEM(F("</td>"));
  HTML_Send_PROGMEM(F("</tr><tr>"));
  HTML_Send_PROGMEM(F("<td></td><td></td><td></td><td></td><td>"));
  HTML_Send_PROGMEM(F("<BUTTON TYPE=\"SUBMIT\" NAME=\"MOVEY\" VALUE=\"0.1\" STYLE=\"Width:60\" WIDTH=\"60\" >Y+0.1</BUTTON>"));
  HTML_Send_PROGMEM(F("</td><td></td><td></td><td></td><td></td><td>"));
  HTML_Send_PROGMEM(F("<BUTTON TYPE=\"SUBMIT\" NAME=\"MOVEZ\" VALUE=\"0.1\" STYLE=\"Width:60\" WIDTH=\"60\" >Z+0.1</BUTTON>"));
  HTML_Send_PROGMEM(F("</td>"));
  HTML_Send_PROGMEM(F("</tr><tr>"));
  HTML_Send_PROGMEM(F("<td>"));
  HTML_Send_PROGMEM(F("<BUTTON TYPE=\"SUBMIT\" NAME=\"MOVEX\" VALUE=\"-100\">X-100</BUTTON>"));
  HTML_Send_PROGMEM(F("</td><td>"));
  HTML_Send_PROGMEM(F("<BUTTON TYPE=\"SUBMIT\" NAME=\"MOVEX\" VALUE=\"-10\">X-10</BUTTON>"));
  HTML_Send_PROGMEM(F("</td><td>"));
  HTML_Send_PROGMEM(F("<BUTTON TYPE=\"SUBMIT\" NAME=\"MOVEX\" VALUE=\"-1\">X-1</BUTTON>"));
  HTML_Send_PROGMEM(F("</td><td>"));
  HTML_Send_PROGMEM(F("<BUTTON TYPE=\"SUBMIT\" NAME=\"MOVEX\" VALUE=\"-0.1\">X-0.1</BUTTON>"));
  HTML_Send_PROGMEM(F("</td><td>"));
  HTML_Send_PROGMEM(F("<BUTTON TYPE=\"SUBMIT\" NAME=\"STOP\" VALUE=\"1\" STYLE=\"Width:60\" WIDTH=\"60\">STOP</BUTTON>"));
  HTML_Send_PROGMEM(F("</td><td>"));
  HTML_Send_PROGMEM(F("<BUTTON TYPE=\"SUBMIT\" NAME=\"MOVEX\" VALUE=\"0.1\">X+0.1</BUTTON>"));
  HTML_Send_PROGMEM(F("</td><td>"));
  HTML_Send_PROGMEM(F("<BUTTON TYPE=\"SUBMIT\" NAME=\"MOVEX\" VALUE=\"1\">X+1</BUTTON>"));
  HTML_Send_PROGMEM(F("</td><td>"));
  HTML_Send_PROGMEM(F("<BUTTON TYPE=\"SUBMIT\" NAME=\"MOVEX\" VALUE=\"10\">X+10</BUTTON>"));
  HTML_Send_PROGMEM(F("</td><td>"));
  HTML_Send_PROGMEM(F("<BUTTON TYPE=\"SUBMIT\" NAME=\"MOVEX\" VALUE=\"100\">X+100</BUTTON>"));
  HTML_Send_PROGMEM(F("</td><td></td>"));
  HTML_Send_PROGMEM(F("</tr><tr>"));
  HTML_Send_PROGMEM(F("<td></td><td></td><td></td><td></td><td>"));
  HTML_Send_PROGMEM(F("<BUTTON TYPE=\"SUBMIT\" NAME=\"MOVEY\" VALUE=\"-0.1\" STYLE=\"Width:60\" WIDTH=\"60\">Y-0.1</BUTTON>"));
  HTML_Send_PROGMEM(F("</td><td></td><td></td><td></td><td></td><td>"));
  HTML_Send_PROGMEM(F("<BUTTON TYPE=\"SUBMIT\" NAME=\"MOVEZ\" VALUE=\"-0.1\" STYLE=\"Width:60\" WIDTH=\"60\" >Z-0.1</BUTTON>"));
  HTML_Send_PROGMEM(F("</td>"));
  HTML_Send_PROGMEM(F("</tr><tr>"));
  HTML_Send_PROGMEM(F("<td></td><td></td><td></td><td></td><td>"));
  HTML_Send_PROGMEM(F("<BUTTON TYPE=\"SUBMIT\" NAME=\"MOVEY\" VALUE=\"-1\" STYLE=\"Width:60\" WIDTH=\"60\">Y-1</BUTTON>"));
  HTML_Send_PROGMEM(F("</td><td></td><td></td><td></td><td></td><td>"));
  HTML_Send_PROGMEM(F("<BUTTON TYPE=\"SUBMIT\" NAME=\"MOVEZ\" VALUE=\"-1\" STYLE=\"Width:60\" WIDTH=\"60\" >Z-1</BUTTON>"));
  HTML_Send_PROGMEM(F("</td>"));
  HTML_Send_PROGMEM(F("</tr><tr>"));
  HTML_Send_PROGMEM(F("<td></td><td></td><td></td><td></td><td>"));
  HTML_Send_PROGMEM(F("<BUTTON TYPE=\"SUBMIT\" NAME=\"MOVEY\" VALUE=\"-10\" STYLE=\"Width:60\" WIDTH=\"60\">Y-10</BUTTON>"));
  HTML_Send_PROGMEM(F("</td><td></td><td></td><td></td><td></td><td>")); 
  HTML_Send_PROGMEM(F("<BUTTON TYPE=\"SUBMIT\" NAME=\"MOVEZ\" VALUE=\"-10\" STYLE=\"Width:60\" WIDTH=\"60\" >Z-10</BUTTON>"));
  HTML_Send_PROGMEM(F("</td>"));
  HTML_Send_PROGMEM(F("</tr><tr>"));
  HTML_Send_PROGMEM(F("<td></td><td></td><td></td><td></td><td>"));
  HTML_Send_PROGMEM(F("<BUTTON TYPE=\"SUBMIT\" NAME=\"MOVEY\" VALUE=\"-100\" STYLE=\"Width:60\" WIDTH=\"60\">Y-100</BUTTON>"));
  HTML_Send_PROGMEM(F("</td><td></td><td></td><td></td><td></td>"));
  HTML_Send_PROGMEM(F("</tr>"));  
  HTML_Send_PROGMEM(F("</table>"));
  HTML_Send_PROGMEM(F("</FORM>"));
  
  // form - FAN
  HTML_Send_PROGMEM(F("<FORM ACTION=\"http://"));
  HTML_Send(WIFI_Host);
  HTML_Send_PROGMEM(F("\">"));

  HTML_Send_PROGMEM(F("<P><FONT size=\"3\" FACE=\"Verdana\">Fan control :<BR/>"));

  HTML_Send_PROGMEM(F("<INPUT TYPE=\"RADIO\" NAME=\"FAN\" VALUE=\"On\"> On<BR/>"));
  HTML_Send_PROGMEM(F("<INPUT TYPE=\"RADIO\" NAME=\"FAN\" VALUE=\"Off\" CHECKED> Off<BR/>"));
  HTML_Send_PROGMEM(F("<BR/>"));
  HTML_Send_PROGMEM(F("<INPUT TYPE=\"SUBMIT\" VALUE=\" Send \">"));
  HTML_Send_PROGMEM(F("</FONT></P></FORM>"));
  
  // generic gcode input_iterator
  HTML_Send_PROGMEM(F("<FORM ACTION=\"http://"));
  HTML_Send(WIFI_Host);
  HTML_Send_PROGMEM(F("\">"));
  HTML_Send_PROGMEM(F("<P>Enter GCode :<BR/>"));
  HTML_Send_PROGMEM(F("<INPUT TYPE=\"Text\" NAME=\"GCODE\">"));
  HTML_Send_PROGMEM(F("<INPUT TYPE=\"SUBMIT\" VALUE=\" Send \">"));
  HTML_Send_PROGMEM(F("</P></FORM>"));
  
  HTML_Send_PROGMEM(F("</BODY></HTML>"));
}

//---------------------------------------------------------------------------
void HTML_Send_Int(int p_int,int width) 
{
  char tmp_text[8];
  sprintf(tmp_text,"%*d",width,p_int);
  HTML_Send(tmp_text);
}

//---------------------------------------------------------------------------
void HTML_Send(char * p_text) 
{
  int len = strlen(p_text);
  HTML_Temp_Length += len;
 
  if (HTML_Send_Mode == HTML_SEND_MODE_SEND) 
  {
	for(int i=0; i < len; i++)
	{
		wifi.write(p_text[i]);
		HTML_Sent_Length++;
		 
		// send new header if needed
		if(HTML_Sent_Length >= HTML_CHUNK_LENGTH)
		{
			HTML_Send_Length -= HTML_Sent_Length;
			HTML_Sent_Length =0;
			if(HTML_Send_Length > HTML_CHUNK_LENGTH)
				ESP8266_Send_Header(WIFI_Channel,HTML_CHUNK_LENGTH,false);
			else 
				ESP8266_Send_Header(WIFI_Channel,HTML_Send_Length,false);
		}
	}
	delay(1); 
  }
}

//---------------------------------------------------------------------------
void HTML_Send_PROGMEM(const __FlashStringHelper* p_text) 
{
  int len =strlen_P((const char*)p_text);
  HTML_Temp_Length += len;
  
  if (HTML_Send_Mode == HTML_SEND_MODE_SEND) 
  {
	for(int i=0; i < len; i++)
	{
		char ch=pgm_read_byte(((const char*)p_text)+i);
		wifi.write(ch);
		HTML_Sent_Length++;
		 
		// send new header if needed
		if(HTML_Sent_Length >= HTML_CHUNK_LENGTH)
		{
			HTML_Send_Length -= HTML_Sent_Length;
			HTML_Sent_Length =0;
			if(HTML_Send_Length > HTML_CHUNK_LENGTH)
				ESP8266_Send_Header(WIFI_Channel,HTML_CHUNK_LENGTH,false);
			else 
				ESP8266_Send_Header(WIFI_Channel,HTML_Send_Length,false);
		}
	}
	delay(1); 
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
		  MYSERIAL.print("waited for");
		  MYSERIAL.println(millis() - _startMillis);
          return true;
        }
      }
    }
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

bool getIP() 
{
  
  char c;
  char buf[15];
  int dots, ptr = 0;
  bool ret = false;

  memset(buf, 0, 15);

  wifi.println(F("AT+CIFSR"));
  delay(500);
  while (wifi.available() > 0) {
    c = wifi.read();
    
    // increment the dot counter if we have a "."
    if ((int)c == 46) {
      dots++;
    }
    if ((int)c == 10) {
      // end of a line.
      if ((dots == 3) && (ret == false)) {
        buf[ptr] = 0;
        strcpy(_ipaddress, buf);
        ret = true;
      } else {
        memset(buf, 0, 15);
        dots = 0;
        ptr = 0;
      }
    } else
    if ((int)c == 13) {
      // ignore it
    } else {
      buf[ptr] = c;
      ptr++;
    }
  }

  return ret;
}

void clearResults() 
{
   for (int i = 0; i < RECV_BUFFER_SIZE; i++) RECV_Buffer[i] = 0;
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
