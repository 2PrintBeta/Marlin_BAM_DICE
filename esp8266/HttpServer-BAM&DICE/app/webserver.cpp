
#include <SmingCore/SmingCore.h>
#include "../include/configuration.h"
#include "../include/webserver.h"
#include "../include/arduino_com.h"

HttpServer server;
FTPServer ftp;

int postState=0;
int postDataProcessed =0;
String boundary;
String postError;

bool uploadInProgress = false;
BAMState curState;

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
	if (request.getRequestMethod() == RequestMethod::GET)
	{
		String query;
		if(request.getQueryParameter("status").length() > 0)
		{
			JsonObjectStream* stream = new JsonObjectStream();
			JsonObject& json = stream->getRoot();
			json["Temp1"] = curState.temp1.c_str();
			json["Temp1Target"] = curState.temp1Target.c_str();
			json["Temp2"] = curState.temp2.c_str();
			json["Temp2Target"] = curState.temp2Target.c_str();
			json["Bed"] = curState.tempBed.c_str();
			json["BedTarget"] = curState.tempBedTarget.c_str();
			json["xPos"] = curState.xPos.c_str();
			json["yPos"] = curState.yPos.c_str();
			json["zPos"] = curState.zPos.c_str();
			json["SDpercent"] = curState.SDpercent.c_str();
			json["printTime"] = curState.printTime.c_str();
			json["SDselected"] = curState.SDselected.c_str();

			json["SDinserted"] = curState.SDinserted;
			json["NumSDFiles"] = curState.numSDEntries;
			JsonArray& files = json.createNestedArray("Files");
			for(int i = 0; i < curState.numSDEntries; i++)
			{
				files.add(curState.SDEntries[i].c_str());
			}

			json["SSID"] = ActiveConfig.NetworkSSID.c_str();
			json["PWD"] = ActiveConfig.NetworkPassword.c_str();
			json["MODE"] = ActiveConfig.mode.c_str();
			json["SEC"] = ActiveConfig.security.c_str();

			response.sendJsonObject(stream);

		}
		else response.forbidden();
	}
	else response.forbidden();
}


void onSet(HttpRequest &request, HttpResponse &response)
{
	if(uploadInProgress)
	{
		response.forbidden();
		return;
	}
	if (request.getRequestMethod() == RequestMethod::GET)
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
		//check answer
		JsonObjectStream* stream = new JsonObjectStream();
		JsonObject& json = stream->getRoot();
		if(cmd_failed == false)
		{
			json["error"] = "0";
			json["message"] = "ok";
		}
		else
		{
			json["error"] = "1";
			json["message"] = cmd_error_str.c_str();
		}
		response.sendJsonObject(stream);

	}
	else response.forbidden();
}

void onUpload(HttpRequest &request, HttpResponse &response)
{
	if(uploadInProgress)
	{
		response.forbidden();
		return;
	}
	if (request.getRequestMethod() == RequestMethod::POST)
	{
		if(postError != "")
		{
			response.setContentType(ContentType::HTML);
			response.sendString("Upload Failed.");
			response.sendString(postError);
		}
		else
		{
			response.setContentType(ContentType::HTML);
			response.sendString("Upload Ok.");
		}
	}
	else response.forbidden();
}

bool onPost(HttpRequest &request,pbuf* buf)
{
	int start = 0;
	int header_end =0;
	// if we are called the first time, get start of data
	if(postState ==0)
	{
		//look for boundary
		int boundaryPos = NetUtils::pbufFindStr(buf, "boundary=");
		if (boundaryPos == -1)
		{
			postError = "Could not find boundary";
			//reset state
			postDataProcessed = 0;
			postState =0;
			return true;
		}
		boundaryPos +=9; //skip unwanted parts
		int boundaryEnd = NetUtils::pbufFindStr(buf,"\r",boundaryPos);
		boundary = "--";
		boundary.concat(NetUtils::pbufStrCopy(buf,boundaryPos,boundaryEnd-boundaryPos));
		boundary.concat("--");

		//look for header end
		header_end= NetUtils::pbufFindStr(buf, "\r\n\r\n");
		if (header_end == -1)
		{
			postError = "Could not find header";
			//reset state
			postDataProcessed = 0;
			postState =0;
			return true;
		}
		header_end = header_end + 4;

		// look for filename
		int namePos = NetUtils::pbufFindStr(buf, "filename=", header_end);
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
			return true;
		}
		start +=test.length();
	}

	// add data length
	postDataProcessed += buf->tot_len - header_end;
	//check if we got all data
	if (postDataProcessed == request.getContentLength())
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
			return true;
		}

		//reset state
		postDataProcessed = 0;
		postState =0;
		postError = "";
		uploadInProgress = false;
		return true;
	}
	else
		return false;
}

void onConfiguration(HttpRequest &request, HttpResponse &response)
{
	BAMConfig cfg = loadConfig();
	if (request.getRequestMethod() == RequestMethod::POST)
	{
		// Update config
		if (request.getPostParameter("SSID").length() > 0) // Network
		{
			cfg.NetworkSSID = request.getPostParameter("SSID");
			if(request.getPostParameter("Password") > 0) cfg.NetworkPassword = request.getPostParameter("Password");
			cfg.mode = request.getPostParameter("MODE");
			cfg.security = request.getPostParameter("SECURITY");
		}

		saveConfig(cfg);
		response.redirect();
	}

	response.setCache(86400, true); // It's important to use cache for better performance.
	response.sendFile("index.html");
}

void onReboot(HttpRequest &request, HttpResponse &response)
{
	if(uploadInProgress)
	{
		response.forbidden();
		return;
	}
	if (request.getRequestMethod() == RequestMethod::GET)
	{
		if(request.getQueryParameter("reboot").length() > 0)
		{
			response.setContentType(ContentType::HTML);
			response.sendString("Reboot Ok.");
			System.restart();
			return;
		}
	}
	response.forbidden();
}

void startWebServer()
{
	curState.temp1 = "0";
	curState.temp1Target = "0";
	curState.temp2 = "--";
	curState.temp2Target = "--";
	curState.tempBed = "0";
	curState.tempBedTarget = "0";
	curState.xPos ="0";
	curState.yPos ="0";
	curState.zPos ="0";
	curState.SDpercent = "---";
	curState.SDselected = "no";
	curState.printTime = "--:--";

	curState.SDinserted = false;
	curState.numSDEntries =0;

	for(int i=0; i < MAX_FILENAMES; i++)
	{
		curState.SDEntries[i] = "";
	}

	server.listen(80);
	server.addPath("/", onConfiguration);
	server.addPath("/index.html", onConfiguration);
	server.addPath("/upload", onUpload);
	server.addPath("/get", onGet);
	server.addPath("/set", onSet);
	server.addPath("/reboot", onReboot);

	server.setDefaultHandler(onFile);
	server.setPostHandler(onPost);
}

void startFTP()
{
	if (!fileExist("index.html"))
		fileSetContent("index.html", "<h3>Please connect to FTP and upload files from folder 'web' (details in code)</h3>");

	// Start FTP server
	ftp.listen(21);
	ftp.addUser("me", "123"); // FTP account
}
