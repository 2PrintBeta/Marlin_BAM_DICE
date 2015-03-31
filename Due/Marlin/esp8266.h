/*
  esp8266.h - support code for the esp8266 wlan modue
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

#include "Configuration.h"

#ifndef ESP8266_H
#define ESP8266_H

#ifdef HAVE_ESP8266

// init the wifi chip
void init_esp8266();

//regulary call this functions
void handle_esp8266();

//html functions
void HTML_Page(int WIFI_Channel);
void HTML_Make_Header();
void HTML_Make_Content();
void HTML_Send_Int(int p_int,int width);
void HTML_Send(char * p_text);
void HTML_Send_PROGMEM(const __FlashStringHelper* p_text);


//helper functions
void process_get(char* buffer_pointer);
void ESP8266_move(int x, int y, int z);
void ESP8266_Send_Header(int channel,int length,bool first);
bool searchResults(char *target, long timeout, int dbg);
bool getIP();
void clearResults();
void debug(char *msg) ;

#endif

#endif