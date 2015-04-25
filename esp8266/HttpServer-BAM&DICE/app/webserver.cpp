#include "../include/webserver.h"

#include <SmingCore/SmingCore.h>
#include "../include/configuration.h"

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

			json["SSID"] = ActiveConfig.NetworkSSID.c_str();
			json["PWD"] = ActiveConfig.NetworkPassword.c_str();
			if(ActiveConfig.isStation == "yes") json["MODE"] = "STATION";
			else json["MODE"] = "AP";
			json["SEC"] = ActiveConfig.security.c_str();

			response.sendJsonObject(stream);

		}
		else response.forbidden();
	}
	else response.forbidden();
}

void onGetLong(HttpRequest &request, HttpResponse &response)
{
	if(uploadInProgress)
	{
		response.forbidden();
		return;
	}
	if (request.getRequestMethod() == RequestMethod::GET)
	{
		String query;
		if(request.getQueryParameter("files").length() > 0)
		{
			query = "LIST";
		}

		//ask arduino
		if(query.length() > 0)
		{
			Serial.println(query);
			response.setContentType(ContentType::JSON);

			//wait for response
			int starttime = millis();
			while(1)
			{
				String resp = Serial.readStringUntil('\n');

				if(resp.startsWith("END")) break;
				else response.sendString(resp);

				if((millis() - starttime) > 1000)
				{
					response.sendString("{\"SD\":\"timeout\"}");
					break;
				}
			}

		}
		else
		{
			response.setContentType(ContentType::JSON);
			response.sendString("{\"error\":1, \"message\":\"Failed\"}");
		}

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
			query = "SETTEMP ";
			query.concat(request.getQueryParameter("heater"));
			query.concat(" ");
			query.concat(request.getQueryParameter("temp"));

		}
		else if(request.getQueryParameter("home").length() > 0 )
		{
			query = "HOME ";
			query.concat(request.getQueryParameter("home"));
		}
		else if(request.getQueryParameter("delete").length() > 0 )
		{
			query = "DELETE ";
			query.concat(request.getQueryParameter("delete"));
		}
		else if(request.getQueryParameter("print").length() > 0 )
		{
			query = "PRINT ";
			query.concat(request.getQueryParameter("print"));
		}
		else if(request.getQueryParameter("pause").length() > 0 )
		{
			query = "PAUSE";
		}
		else if(request.getQueryParameter("resume").length() > 0 )
		{
			query = "RESUME";
		}
		else if(request.getQueryParameter("stop").length() > 0 )
		{
			query = "STOP";
		}
		else if(request.getQueryParameter("move").length() > 0 &&
		   request.getQueryParameter("axis").length() > 0 &&
		   request.getQueryParameter("speed").length() > 0)
		{
			switch(request.getQueryParameter("axis").toInt())
			{
				case 1:
					query = "MOVE ";
					query.concat(request.getQueryParameter("move"));
					query.concat(" 0 0 0 ");
					query.concat(request.getQueryParameter("speed"));
					break;
				case 2:
					query = "MOVE 0 ";
					query.concat(request.getQueryParameter("move"));
					query.concat(" 0 0 ");
					query.concat(request.getQueryParameter("speed"));
					break;
				case 3:
					query = "MOVE 0 0 ";
					query.concat(request.getQueryParameter("move"));
					query.concat(" 0 ");
					query.concat(request.getQueryParameter("speed"));
					break;
				case 4:
					query = "MOVE 0 0 0 ";
					query.concat(request.getQueryParameter("move"));
					query.concat(" ");
					query.concat(request.getQueryParameter("speed"));
					break;
				default:
					break;
			}
		}
		//ask arduino
		if(query.length() > 0)
		{
			Serial.println(query);
			//wait for answer
			String resp = Serial.readStringUntil('\n');
			if(resp.indexOf("ok") != -1)
			{
				response.setContentType(ContentType::JSON);
				response.sendString("{\"error\":0, \"message\":\"OK\"}");
			}
			else
			{
				response.setContentType(ContentType::JSON);
				response.sendString("{\"error\":1, \"message\":\"Failed\"}");
			}
		}
		else response.forbidden();
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
		Serial.print("UPLOAD ");
		Serial.println(filename);
		//wait for response
		String response = Serial.readStringUntil('\n');
		if(response.indexOf("ok") == -1)
		{
			postError = response;
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
		Serial.print(test.c_str());
		Serial.write(0);
		//wait for response
		String response = Serial.readStringUntil('\n');
		if(response.indexOf("ok") == -1)
		{
			postError = response;
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
		//close file
		Serial.println("ENDUPLOAD");
		Serial.write(0);
		//wait for response
		String response = Serial.readStringUntil('\n');
		if(response.indexOf("ok") == -1)
		{
			// Error handling
			postError = response;
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
			cfg.isStation = request.getPostParameter("STATION");
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

	server.listen(80);
	server.addPath("/", onConfiguration);
	server.addPath("/index.html", onConfiguration);
	server.addPath("/upload", onUpload);
	server.addPath("/get", onGet);
	server.addPath("/getLong", onGetLong);
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
