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

// helper
void check_upload_esp8266(int type);


//helper functions
void upload_file_esp8266(char* remote_file);
void esp8266_reset();

void ESP8266_move(int x, int y, int z);
bool searchResults(char *target, long timeout, int dbg);
void clearResults();
void debug(char *msg) ;

#endif

#endif