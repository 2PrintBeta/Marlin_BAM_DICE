#include <user_config.h>
#include <SmingCore/SmingCore.h>
#include "../include/arduino_com.h"
#include "../include/webserver.h"
#include "../include/configuration.h"

volatile bool cmd_failed;
String cmd_error_str;

struct comm_package
{
	ESP_CMDs cmd;
	unsigned char length;
	unsigned char data[61];
	unsigned char checksum;  //xor sum from cmd till data end
};

//buffers for the commands/answers
comm_package esp_cmd;
comm_package esp_answer;
int esp_data_index=0;

//data struct for state cmd
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

// types of idle command lists
enum IDLE_CMD_TYPE
{
	eStd,  //normal commands
	eNetwork, //used when signaled that network changed
	eSD, //used when signaled that SD changed to plugged
};
//current list of idle cmds
IDLE_CMD_TYPE idle_cmd_type = eStd;

ESP_CMDs idle_cmds_std [] = {
	eSetNetworkSSID,
	eSetNetworkMode,
	eSetNetworkIP,
};

ESP_CMDs idle_cmds_net [] = {
	eGetNetworkSSID,
	eGetNetworkPWD,
	eGetNetworkMode,
	eGetNetworkSec,  //it is important that this is the last of the network cmds
};

ESP_CMDs idle_cmds_sd [] = {
	eGetNumSDEntries, //these are not always called, depending on previous answers
	eGetSDEntry, //these are not always called, depending on previous answers
};

int idle_cmd_index =0;
int sd_entry_num =0;

enum ESP_COMM_STATE
{
	eIdle,
	eHeader,
	eData,
	eChecksum,
};

enum ESP_CMD_STATE
{
	eCmdIdle,
	eCmdProcess,
};
ESP_COMM_STATE comm_state = eIdle;
ESP_CMD_STATE cmd_state = eCmdIdle;

unsigned long cmd_start_time=0;
#define CMD_TIMEOUT 500
#define CRC_MAGIC 0xE

volatile bool in_sync = false;
///////////////////////
// Helper functions
////////////////////////
void handle_answer();
unsigned char calc_crc(comm_package* pkt);
void reset();
void send_next_cmd();
void esp_send();
void esp_send_debug(char* dbg);
///////////////////////////
// handle arduino comm
///////////////////////////
void handle_arduino()
{
	if(cmd_state == eCmdIdle)
	{
		//send next command
		send_next_cmd();
	}

	//process incoming answers
	if(cmd_state == eCmdProcess)
	{
		bool answ_ready = false;

		//read from Arduino serial, as long as there are chars available
		while(Serial.available())
		{
			unsigned char c = Serial.read();

			switch(comm_state)
			{
				case eIdle:
					esp_answer.cmd=(ESP_CMDs)c;
					//TODO check for valid command ?
					esp_data_index =0;
					comm_state = eHeader;
					break;
				case eHeader:
					esp_answer.length = c;
					if(esp_answer.length > 60)
					{
						//invalid length
						esp_send_debug((char*)"invalid length");
						reset();
					}
					if(esp_answer.length > 0) comm_state = eData;
					else comm_state = eChecksum;
					break;
				case eData:
					esp_answer.data[esp_data_index] = c;
					esp_data_index++;
					if(esp_data_index >= esp_answer.length) comm_state = eChecksum;
					break;
				case eChecksum:
					esp_answer.checksum = c;
					comm_state = eIdle;
					answ_ready = true;
					break;
			}
			// stop reading if we have a full command
			if(answ_ready) break;
		}

		// we have a cmd in buffer, handle it
		if(answ_ready)
		{
			handle_answer();
			cmd_state = eCmdIdle;
		}
	}

	//check for timeouts
	if(cmd_state != eCmdIdle)
	{
		if((millis() -cmd_start_time) > CMD_TIMEOUT)
		{
			// cmd timeout
			esp_send_debug((char*)"cmd timeout");
			reset();
		}
	}
}


bool network_changed = false;
void handle_answer()
{

	//check checksum
	unsigned char checksum = calc_crc(&esp_answer);
	if(esp_answer.checksum != checksum)
	{
		// Invalid CRC recieved from Arduino
		esp_send_debug((char*)"invalid crc");
		return;
	}

	//react to answer
	if(esp_answer.cmd == eOk)
	{
		cmd_failed = false;
		cmd_error_str = "";
	}
	else
	{
		cmd_failed = true;
		esp_answer.data[esp_answer.length]=0;
		cmd_error_str = (char*)esp_answer.data;
		return;
	}
/*
	String test = "answ:";
	test.concat((int)esp_answer.cmd );
	test.concat(" ");
	test.concat((int)esp_answer.length );
	esp_answer.data[esp_answer.length] =0;
	test.concat((char*)esp_answer.data);
	test.concat(" ");
	test.concat((int)esp_answer.checksum);
	esp_send_debug((char*)test.c_str());
*/
	//switch depending on current cmd
	switch(esp_cmd.cmd)
	{
		case eSync:
		{
			if(esp_answer.cmd == eOk) in_sync = true;
			else esp_send_debug((char*)"sync returned error ?");
			break;
		}
		case eSetNetworkMode:
		case eSetNetworkSSID:
		case eSetNetworkIP:
		{
			S_STATE* data = (S_STATE*) esp_answer.data;

			//file selected
			curState.SDselected = data->fileOpen;

			//print percentage
			curState.SDpercent = data->percentage;

			//print time
			curState.printTime=data->printtime;

			//temps
			curState.temp1 = data->temp1;
			curState.temp1Target = data->temp1Target;
			curState.temp2 = data->temp2;
			curState.temp2Target = data->temp2Target;
			curState.tempBed = data->tempBed;
			curState.tempBedTarget = data->tempBedTarget;
			//position
			curState.xPos = data->x;
			curState.yPos = data->y;
			curState.zPos = data->z;

			//sd plugged
			if(data->sdPlugged && !curState.SDinserted) //change idle cmd
			{
				idle_cmd_type = eSD;
				idle_cmd_index =0;
			}
			curState.SDinserted =data->sdPlugged;

			//network changed
			if(data->networkChanged)
			{
				idle_cmd_type = eNetwork;
				idle_cmd_index =0;
			}

			//fan Speed
			curState.fanSpeed = data->fanSpeed;
			break;
		}
		case eGetNetworkSSID:
		{
			if(esp_answer.length > 0)
			{
				esp_answer.data[esp_answer.length]=0;
				String ssid = (char*)esp_answer.data;
				ssid.trim();
				if(ssid.length() > 0 && ActiveConfig.NetworkSSID != ssid)
				{
					ActiveConfig.NetworkSSID = ssid;
					network_changed = true;
					esp_send_debug((char*)"ssid changed");
				}
			}
			break;
		}
		case eGetNetworkPWD:
		{
			if(esp_answer.length > 0)
			{
				esp_answer.data[esp_answer.length]=0;
				String pwd = (char*)esp_answer.data;
				pwd.trim();
				if(pwd.length()>0 && ActiveConfig.NetworkPassword != pwd)
				{
					ActiveConfig.NetworkPassword = pwd;
					network_changed = true;
					esp_send_debug((char*)"pwd changed");
				}
			}
			break;
		}
		case eGetNetworkMode:
		{
			if(esp_answer.length > 0)
			{
				esp_answer.data[esp_answer.length]=0;
				String mode = (char*)esp_answer.data;
				mode.trim();
				if(mode.length() > 0 && ActiveConfig.mode != mode)
				{
					ActiveConfig.mode = mode;
					network_changed = true;
					esp_send_debug((char*)"mode changed");
				}
			}
			break;
		}
		case eGetNetworkSec:
		{
			if(esp_answer.length > 0)
			{
				esp_answer.data[esp_answer.length]=0;
				String sec = (char*)esp_answer.data;
				sec.trim();
				if(sec.length() > 0 && ActiveConfig.security != sec)
				{
					ActiveConfig.security = sec;
					network_changed = true;
					esp_send_debug((char*)"sec changed");
				}
			}

			//Network Sec is always the last of the network cmds
			if(network_changed)
			{
				esp_send_debug((char*)"rebooting");
				saveConfig(ActiveConfig);
				System.restart();
			}
			break;
		}
		case eGetNumSDEntries:
		{
			memcpy(&curState.numSDEntries,esp_answer.data,2);
			if( curState.numSDEntries > MAX_FILENAMES) curState.numSDEntries = MAX_FILENAMES;

			break;
		}
		case eGetSDEntry:
		{
			uint16_t file_index;
			memcpy(&file_index,esp_cmd.data,2);  // copy from requested cmd

			// only store max filenames
			if(file_index > MAX_FILENAMES) break;

			// ignore too long filenames
			if(esp_answer.length > STORAGE_SIZE) break;
			// Store data
			memcpy(curState.SDEntries[file_index],esp_answer.data,esp_answer.length);
			curState.SDEntries[file_index][esp_answer.length] =0;

			break;
		}
		case eDelete:
		case eCloseFile:
		{
			//refresh file list after deletion or creation
			idle_cmd_type = eSD;
			idle_cmd_index =0;
			break;
		}
		default:
			// Unknown command recieved from Arduino
			break;

	}
}

void sendActiveCmd(ESP_CMDs cmd,int len,unsigned char* data)
{
	if(in_sync == false)
	{
		cmd_failed = true;
		cmd_error_str = "Not in sync";
		return;
	}
	//wait till any pending cmd is finished
	while(cmd_state != eCmdIdle)
	{
		handle_arduino();
	}

	//create cmd
	esp_cmd.cmd = cmd;
	esp_cmd.length = len;
	memcpy(esp_cmd.data,data,len);

	//send cmd
	esp_send();
	//wait till cmd is handled
	while(cmd_state != eCmdIdle)
	{
		handle_arduino();
	}
}

void send_next_cmd()
{
	//if we are not in sync, send sync command
	if(in_sync == false)
	{
		esp_cmd.cmd = eSync;
		esp_cmd.length =0;
	}
	else
	{
		switch(idle_cmd_type)
		{
			case eStd:
				// fill in current cmd
				esp_cmd.cmd = idle_cmds_std[idle_cmd_index];
				esp_cmd.length =0;
				// fill in data if needed
				switch(esp_cmd.cmd)
				{
					case eSetNetworkSSID:
						esp_cmd.length = ActiveConfig.NetworkSSID.length();
						memcpy(esp_cmd.data,ActiveConfig.NetworkSSID.c_str(), ActiveConfig.NetworkSSID.length());
						break;
					case eSetNetworkMode:
						esp_cmd.length = ActiveConfig.mode.length();
						memcpy(esp_cmd.data,ActiveConfig.mode.c_str(), ActiveConfig.mode.length());
						break;
					case eSetNetworkIP:
						esp_cmd.length = ActiveConfig.ip.length();
						memcpy(esp_cmd.data,ActiveConfig.ip.c_str(), ActiveConfig.ip.length());
						break;

					default:
						//nothing todo
						break;
				}

				idle_cmd_index++;
				//check if we are at the end of the list
				if(idle_cmd_index >= (sizeof(idle_cmds_std)/sizeof(idle_cmds_std[0])))idle_cmd_index =0;

			break;
			case eNetwork:

				esp_cmd.cmd = idle_cmds_net[idle_cmd_index];
				esp_cmd.length =0;
				// no data needed

				idle_cmd_index++;
				//check if we are at the end of the list
				if(idle_cmd_index >= (sizeof(idle_cmds_net)/sizeof(idle_cmds_net[0])))
				{
					idle_cmd_index =0;
					idle_cmd_type = eStd;
				}

			break;
			case eSD:
				esp_cmd.cmd = idle_cmds_sd[idle_cmd_index];
				esp_cmd.length =0;
				// fill in data if needed
				switch(esp_cmd.cmd)
				{
					case eGetSDEntry:
						esp_cmd.length = 2;
						memcpy(esp_cmd.data,&sd_entry_num,2);
						sd_entry_num++;

						//only increment cmd_index if all files are read
						if(sd_entry_num >= curState.numSDEntries) idle_cmd_index++;
						break;
					default:
						idle_cmd_index++;
						break;
				}

				//check if we are at the end of the list
				if(idle_cmd_index >= (sizeof(idle_cmds_sd)/sizeof(idle_cmds_sd[0])))
				{
					idle_cmd_index =0;
					idle_cmd_type = eStd;
					sd_entry_num =0;
				}

			break;
		}
	}

	esp_send();
}

void esp_send()
{
	// create checksum
	esp_cmd.checksum = calc_crc(&esp_cmd);
	//send packet
	Serial.write(esp_cmd.cmd);
	Serial.write(esp_cmd.length);
	for(int i=0;i < esp_cmd.length; i++)
	{
		Serial.write(esp_cmd.data[i]);
	}
	Serial.write(esp_cmd.checksum);

	//change state
	cmd_state = eCmdProcess;
	cmd_start_time = millis(); //remember start time for timeouts
}

void esp_send_debug(char *dbg)
{
	comm_package cmd;
	cmd.cmd = eDebug;
	cmd.length = strlen(dbg);
	memcpy(cmd.data,dbg,cmd.length);
	cmd.checksum = calc_crc(&cmd);
	//send packet
	Serial.write(cmd.cmd);
	Serial.write(cmd.length);
	for(int i=0;i < cmd.length; i++)
	{
		Serial.write(cmd.data[i]);
	}
	Serial.write(cmd.checksum);
}

void reset()
{
	comm_state = eIdle;
	cmd_state = eCmdIdle;
	in_sync = false;

	cmd_failed = false;
	cmd_error_str = "";

	idle_cmd_index =0;
	sd_entry_num =0;
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
