#include "../include/configuration.h"

#include <SmingCore/SmingCore.h>


BAMConfig ActiveConfig;

BAMConfig loadConfig()
{
	DynamicJsonBuffer jsonBuffer;
	BAMConfig cfg;
	if(fileExist(CONFIG_FILE))
	{
		int size = fileGetSize(CONFIG_FILE);
		char* jsonString = new char[size + 1];
		fileGetContent(CONFIG_FILE, jsonString, size + 1);
		JsonObject& root = jsonBuffer.parseObject(jsonString);

		JsonObject& network = root["network"];
		cfg.NetworkSSID = String((const char*)network["ssid"]);
		cfg.NetworkPassword = String((const char*)network["password"]);
		cfg.isStation = String((const char*)network["isStation"]);
		cfg.security = String((const char*)network["security"]);
		delete[] jsonString;
	}
	else
	{
		/*
		//TEST
		cfg.NetworkSSID = "BETA-NET";
		cfg.NetworkPassword = "8127969022774633";
		cfg.isStation = "yes";
		cfg.security = "OPEN";
	*/


		cfg.NetworkSSID = "BAM&DICE";
		cfg.NetworkPassword = "";
		cfg.isStation = "no";
		cfg.security = "OPEN";

	}
	return cfg;
}

void saveConfig(BAMConfig& cfg)
{
	ActiveConfig = cfg;

	DynamicJsonBuffer jsonBuffer;
	JsonObject& root = jsonBuffer.createObject();

	JsonObject& network = jsonBuffer.createObject();
	root["network"] = network;
	network["ssid"] = cfg.NetworkSSID.c_str();
	network["password"] = cfg.NetworkPassword.c_str();
	network["isStation"] = cfg.isStation.c_str();
	network["security"] = cfg.security.c_str();

	char buf[3048];
	root.prettyPrintTo(buf, sizeof(buf));
	fileSetContent(CONFIG_FILE, buf);
}


