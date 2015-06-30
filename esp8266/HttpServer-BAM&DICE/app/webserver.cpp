
#include <SmingCore/SmingCore.h>
#include "../include/configuration.h"
#include "../include/webserver.h"
#include "../include/arduino_com.h"

HttpServer server;
FTPServer ftp;

const char* defaultContent = "<!DOCTYPE html> \
<html> \
   <head> \
      <meta charset=\"UTF-8\"> \
      <title>BAM&amp;DICE Control </title> \
	  <script>  \
      function reboot() \
       { \
     	var url = \"/setinternal?reboot=1\"; \
     	xmlHttp = new XMLHttpRequest(); \
     	xmlHttp.open(\"GET\", url, true); \
     	xmlHttp.send( null ); \
       } \
       function saveConfig() \
       { \
     	var url = \"/setinternal?network=\"; \
     	url = url + document.getElementById(\"wifi_ssid\").value; \
     	url = url+ \"&pwd=\"; \
     	url = url + document.getElementById(\"wifi_pwd\").value; \
     	url = url+ \"&mode=\"; \
     	url = url+ document.getElementById(\"wifi_mode\").options[document.getElementById(\"wifi_mode\").selectedIndex].text; \
     	url = url+ \"&sec=\"; \
     	url = url+ document.getElementById(\"wifi_sec\").options[document.getElementById(\"wifi_sec\").selectedIndex].text; \
     	xmlHttp = new XMLHttpRequest(); \
     	xmlHttp.open(\"GET\", url, true); \
     	xmlHttp.send( null ); \
       } \
      </script> \
   </head> \
   <body> \
   <h3> No html files found. </h3> \
	Configure this device for internet accesss, and it will download them  \
   <form> \
   <table> \
   <tr> <td> Wifi name </td> \
        <td> <input type=\"text\" id=\"wifi_ssid\" placeholder=\"Enter SSID\" value=\"none\"> </td> \
   </tr> <tr> \
   <td> Wifi password </td> \
     <td> <input type=\"password\" id=\"wifi_pwd\" placeholder=\"Password\"> </td> \
   </tr><tr> \
   <td> Wifi mode </td> \
     	<td> <select id=\"wifi_mode\" size=\"1\" > <option value=\"yes\">STATION</option> <option value=\"no\">ACCESSPOINT</option> </select> </td> \
   </tr><tr> \
   <td> Wifi security </td> \
     	<td> <select id=\"wifi_sec\" size=\"1\"> \
			<option>OPEN</option> \
		    <option>WEP</option> \
			<option>WPA_PSK</option> \
			<option>WPA2_PSK</option> \
		    <option>WPA_WPA2_PSK</option> \
		</select> </td> \
   </tr> </table> \
   <button type=\"button\" onclick=\"saveConfig()\">Save</button> \
   </form> <br/> \
   <button type=\"button\" onclick=\"reboot()\">Reboot </button> \
   </body> \
</html>";


int postState=0;
int postDataProcessed =0;
String boundary;
String postError;

bool uploadInProgress = false;
BAMState curState;

Timer downloadTimer;
HttpClient downloadClient;
int dowfid = 0;
void downloadContentFiles()
{
	if (downloadClient.isProcessing()) return; // Please, wait.

	if (downloadClient.isSuccessful())
		dowfid++; // Success. Go to next file!
	downloadClient.reset(); // Reset current download status

	if (dowfid == 0)
		downloadClient.downloadFile("http://www.2printbeta.de/download/esp8266/index.html.gz");
	else if (dowfid == 1)
		downloadClient.downloadFile("http://www.2printbeta.de/download/esp8266/chart.min.js.gz");
	else if (dowfid == 2)
		downloadClient.downloadFile("http://www.2printbeta.de/download/esp8266/index.css.gz");
	else if (dowfid == 3)
		downloadClient.downloadFile("http://www.2printbeta.de/download/esp8266/index.js.gz");
	else if (dowfid == 4)
		downloadClient.downloadFile("http://www.2printbeta.de/download/esp8266/logo.gif");
	else
	{
		// Content download was completed
		downloadTimer.stop();
		//delete internal file
		if (fileExist("index.html"))fileDelete("index.html");
	}
}


void onFile(HttpRequest &request, HttpResponse &response)
{

	String file = request.getPath();
	if (file[0] == '/')
		file = file.substring(1);

	if (file[0] == '.')
		response.forbidden();
	else
	{
		response.setCache(86400, true); // It's important to use cache for better performance.
		response.sendFile(file);
	}
}

void onGet(HttpRequest &request, HttpResponse &response)
{
	JsonObjectStream* stream = new JsonObjectStream();
	JsonObject& json = stream->getRoot();

	if(!in_sync)
	{
		json["error"] = "1";
		json["message"] = "Not connected";
	}
	else if (request.getRequestMethod() == RequestMethod::GET)
	{
		String query;
		if(request.getQueryParameter("status").length() > 0)
		{
			json["Temp1"] = curState.temp1;
			json["Temp1Target"] = curState.temp1Target;
			if(curState.temp2 == -100)
			{
				json["Temp2"] = "--";
				json["Temp2Target"] = "--";
			}
			else
			{
				json["Temp2"] = curState.temp2;
				json["Temp2Target"] = curState.temp2Target;
			}
			json["Bed"] = curState.tempBed;
			json["BedTarget"] = curState.tempBedTarget;
			json["xPos"] = curState.xPos;
			json["yPos"] = curState.yPos;
			json["zPos"] = curState.zPos;
			if(curState.SDpercent == 255)json["SDpercent"] = "---";
			else json["SDpercent"] = curState.SDpercent;

			json["printTime"] = curState.printTime;

			if(curState.SDselected)json["SDselected"] = "yes";
			else json["SDselected"] = "no" ;

			json["fanSpeed"] = curState.fanSpeed;
			json["SDinserted"] = curState.SDinserted;
			if(curState.SDinserted)
			{
				json["NumSDFiles"] = curState.numSDEntries;
				JsonArray& files = json.createNestedArray("Files");
				for(int i = 0; i < curState.numSDEntries; i++)
				{
					files.add(curState.SDEntries[i]);
				}
			}
			else
			{
				json["NumSDFiles"] = 0;
				for(int i=0; i < MAX_FILENAMES; i++)
				{
					curState.SDEntries[i][0] = 0;
				}
			}

			json["SSID"] = ActiveConfig.NetworkSSID.c_str();
			json["PWD"] = ActiveConfig.NetworkPassword.c_str();
			json["MODE"] = ActiveConfig.mode.c_str();
			json["SEC"] = ActiveConfig.security.c_str();

			json["error"] = "0";
			json["message"] = "Ok";
		}
		else
		{
			json["error"] = "1";
			json["message"] = "Unknown Request";
		}
	}
	else
	{
		json["error"] = "1";
		json["message"] = "Unknown Request";
	}

	response.sendJsonObject(stream);
}


void onSet(HttpRequest &request, HttpResponse &response)
{
	JsonObjectStream* stream = new JsonObjectStream();
	JsonObject& json = stream->getRoot();

	if(uploadInProgress)
	{
		json["error"] = "1";
		json["message"] = "Upload in progress";
	}
	else if(!in_sync)
	{
		json["error"] = "1";
		json["message"] = "Not connected";
	}
	else if (request.getRequestMethod() == RequestMethod::GET)
	{
		String query;
		if(request.getQueryParameter("heater").length() > 0 &&
		   request.getQueryParameter("temp").length() > 0)
		{
			char heater = request.getQueryParameter("heater").toInt();
			short temp = request.getQueryParameter("temp").toInt();

			unsigned char data[3];
			memcpy(data,&heater,1);
			memcpy(data+1,&temp,2);
			sendActiveCmd(eSetTemp,3,data);
		}
		else if(request.getQueryParameter("home").length() > 0 )
		{
			unsigned char data = request.getQueryParameter("home").toInt();
			sendActiveCmd(eHome,1,&data);
		}
		else if(request.getQueryParameter("delete").length() > 0 )
		{
			char data[60];
			strncpy((char*)data,request.getQueryParameter("delete").c_str(),60);
			data[59] =0;
			sendActiveCmd(eDelete,strlen(data),(unsigned char*)data);

		}
		else if(request.getQueryParameter("print").length() > 0 )
		{
			char data[60];
			strncpy((char*)data,request.getQueryParameter("print").c_str(),60);
			data[59] =0;
			sendActiveCmd(ePrint,strlen(data),(unsigned char*)data);

		}
		else if(request.getQueryParameter("pause").length() > 0 )
		{
			unsigned char data;
			sendActiveCmd(ePause,0,&data);
		}
		else if(request.getQueryParameter("resume").length() > 0 )
		{
			unsigned char data;
			sendActiveCmd(eResume,0,&data);
		}
		else if(request.getQueryParameter("stop").length() > 0 )
		{
			unsigned char data;
			sendActiveCmd(eStop,0,&data);
		}
		else if(request.getQueryParameter("move").length() > 0 &&
		   request.getQueryParameter("axis").length() > 0 &&
		   request.getQueryParameter("speed").length() > 0)
		{
			unsigned char data[20] = {0};
			float speed =request.getQueryParameter("speed").toFloat();
			float amount = request.getQueryParameter("move").toFloat();
			int axis = request.getQueryParameter("axis").toInt();
			if(axis == 1) memcpy(data,&amount,4);
			if(axis == 2) memcpy(data+4,&amount,4);
			if(axis == 3) memcpy(data+8,&amount,4);
			if(axis == 4) memcpy(data+12,&amount,4);
			memcpy(data+16,&speed,4);
			sendActiveCmd(eMove,20,data);

		}
		else if(request.getQueryParameter("fan").length() > 0 )
		{
			unsigned char data = request.getQueryParameter("fan").toInt();
			sendActiveCmd(eSetFan,1,&data);
		}
		else if(request.getQueryParameter("gcode").length() > 0 )
		{
			String data = request.getQueryParameter("gcode");
			if(data.length() > 59) data = data.substring(0,59);
			sendActiveCmd(eGcode,data.length(),(unsigned char*)data.c_str());
		}

		else
		{
			json["error"] = "0";
			json["message"] = "Unknown request";
		}

		//check answer
		if(cmd_failed == false)
		{
			json["error"] = "0";
			json["message"] = "Ok";
		}
		else
		{
			json["error"] = "1";
			json["message"] = cmd_error_str.c_str();
		}
	}
	else
	{
		json["error"] = "1";
		json["message"] = "Unknown request";
	}

	response.sendJsonObject(stream);
}

void onUpload(HttpRequest &request, HttpResponse &response)
{
	JsonObjectStream* stream = new JsonObjectStream();
	JsonObject& json = stream->getRoot();

	if(uploadInProgress)
	{
		json["error"] = "1";
		json["message"] = "Upload in progress";
	}
	else if(!in_sync)
	{
		json["error"] = "1";
		json["message"] = "Not connected";
	}
	else if (request.getRequestMethod() == RequestMethod::POST)
	{
		if(postError != "")
		{
			json["error"] = "1";
			json["message"] = postError;
		}
		else
		{
			json["error"] = "0";
			json["message"] = "Ok";
		}
	}
	else
	{
		json["error"] = "1";
		json["message"] = "Unknown Request";
	}
	response.sendJsonObject(stream);
}

bool onPost(HttpRequest &request,pbuf* buf)
{
	int start = 0;
	int header_end =0;
	// if we are called the first time, get start of data
	if(postState ==0)
	{
		//look for boundary
		if(boundary == "")
		{
			int boundaryPos = NetUtils::pbufFindStr(buf, "boundary=");
			if (boundaryPos == -1)
			{
				postError = "Could not find boundary";
				//reset state
				postDataProcessed = 0;
				postState =0;
				boundary = "";
				return true;
			}
			boundaryPos +=9; //skip unwanted parts
			int boundaryEnd = NetUtils::pbufFindStr(buf,"\r",boundaryPos);
			boundary = "--";
			boundary.concat(NetUtils::pbufStrCopy(buf,boundaryPos,boundaryEnd-boundaryPos));
			boundary.concat("--");
		}

		// look for filename
		int namePos = NetUtils::pbufFindStr(buf, "filename=");
		if(namePos == -1)
		{
			// filename not found, is it in other pakets ?
			postDataProcessed--;
			if(postDataProcessed == -3) return true;
			return false;
		}
		postDataProcessed =0;
		namePos += 10; // length of unwanted string
		int nameEnd = NetUtils::pbufFindStr(buf,"\r",namePos);
		String filename = NetUtils::pbufStrCopy(buf,namePos,nameEnd-namePos-1);

		// find start of actual data
		start = NetUtils::pbufFindStr(buf, "\r\n\r\n",nameEnd);
		start += 4; //length of unwanted string

		// open file
		uploadInProgress = true;
		sendActiveCmd(eOpenFile,filename.length(),(unsigned char*)filename.c_str());
		if(cmd_failed)
		{
			postError = cmd_error_str;
			//reset state
			postDataProcessed = 0;
			postState =0;
			uploadInProgress = false;
			boundary ="";
			return true;
		}

		//header is processed
		postState = 1;
	}
	// process data
	int data_len = buf->tot_len;

	// look for end of packet
	int boundaryPos = NetUtils::pbufFindStr(buf,boundary.c_str(),start);
	if(boundaryPos != -1) data_len = boundaryPos-2;

	//find end of payload (boundary)
	while( start < data_len)
	{
		String test;
		if(start+60 < data_len) test = NetUtils::pbufStrCopy(buf,start,60);
		else test = NetUtils::pbufStrCopy(buf,start,data_len - start);

		//send data
		sendActiveCmd(eFileData,test.length(),(unsigned char*)test.c_str());
		if(cmd_failed)
		{
			postError = cmd_error_str;
			//reset state
			postDataProcessed = 0;
			postState =0;
			uploadInProgress = false;
			boundary = "";
			return true;
		}
		start +=test.length();
	}

	// add data length
	//postDataProcessed += buf->tot_len - header_end;
	//check if we got all data
	if (boundaryPos != -1)
	{
		unsigned char data;
		sendActiveCmd(eCloseFile,0,&data);
		if(cmd_failed)
		{
			postError = cmd_error_str;
			//reset state
			postDataProcessed = 0;
			postState =0;
			uploadInProgress = false;
			boundary = "";
			return true;
		}

		//reset state
		postDataProcessed = 0;
		postState =0;
		postError = "";
		uploadInProgress = false;
		boundary = "";
		return true;
	}
	else
		return false;
}

void onIndex(HttpRequest &request, HttpResponse &response)
{
	response.setCache(86400, true); // It's important to use cache for better performance.
	response.sendFile("index.html");
}

void onSetInternal(HttpRequest &request, HttpResponse &response)
{
	JsonObjectStream* stream = new JsonObjectStream();
	JsonObject& json = stream->getRoot();

	if (request.getRequestMethod() == RequestMethod::GET)
	{
		if(request.getQueryParameter("reboot").length() > 0)
		{
			json["error"] = "0";
			json["message"] = "Ok";
			response.sendJsonObject(stream);

			System.restart();
			return;
		}
		else if(request.getQueryParameter("network").length() > 0 )
		{
			ActiveConfig.NetworkSSID = request.getQueryParameter("network").c_str();
			if(request.getQueryParameter("pwd") > 0) ActiveConfig.NetworkPassword = request.getQueryParameter("pwd");
			ActiveConfig.mode = request.getQueryParameter("mode");
			ActiveConfig.security = request.getQueryParameter("sec");
			saveConfig(ActiveConfig);
		}

		json["error"] = "0";
		json["message"] = "Ok";
	}
	else
	{
		json["error"] = "1";
		json["message"] = "Unknown Request";
	}
	response.sendJsonObject(stream);
}

void startWebServer()
{
	//reset state vars
	curState.temp1 = 0.0;
	curState.temp1Target = 0.0;
	curState.temp2 = -100.0;
	curState.temp2Target = -100.0;
	curState.tempBed =  0.0;
	curState.tempBedTarget = 0.0;
	curState.xPos = 0.0;
	curState.yPos = 0.0;
	curState.zPos = 0.0;
	curState.SDpercent = 255;
	curState.SDselected = false;
	curState.printTime = 0;
    curState.fanSpeed = 0;

	curState.SDinserted = false;
	curState.numSDEntries =0;

	for(int i=0; i < MAX_FILENAMES; i++)
	{
		curState.SDEntries[i][0] = 0;
	}

	//check required files
	if (!fileExist("index.html.gz") ||
		!fileExist("chart.min.js.gz") ||
		!fileExist("index.css.gz") ||
		!fileExist("index.js.gz") ||
		!fileExist("logo.gif"))
	{
		debugf("no files found");
		// if we are station, try to download files
		if(ActiveConfig.mode == "STATION")
		{
			// Download server content at first
			debugf("starting download");
			downloadTimer.initializeMs(3000, downloadContentFiles).start();
		}
		spiffs_format_internal();
		fileSetContent("index.html", defaultContent);
		debugf("index html created");
	}


	// add http handlers
	server.listen(80);
	server.addPath("/", onIndex);
	server.addPath("/index.html", onIndex);
	server.addPath("/upload", onUpload);
	server.addPath("/get", onGet);
	server.addPath("/set", onSet);
	server.addPath("/setinternal", onSetInternal);

	server.setDefaultHandler(onFile);
	server.setPostHandler(onPost);
}

void startFTP()
{
	// Start FTP server
	ftp.listen(21);
	ftp.addUser("me", "123"); // FTP account
}
