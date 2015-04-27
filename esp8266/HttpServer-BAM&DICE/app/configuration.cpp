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
		if(network.containsKey("ssid"))cfg.NetworkSSID = network["ssid"].asString();
		else cfg.NetworkSSID = "BAM&DICE";

		if(network.containsKey("password")) cfg.NetworkPassword = network["password"].asString();
		else cfg.NetworkPassword = "";

		if(network.containsKey("mode")) cfg.mode = network["mode"].asString();
		else cfg.mode = "AP";

		if(network.containsKey("security")) cfg.security = network["security"].asString();
		else cfg.security = "OPEN";

		delete[] jsonString;
	}
	else
	{

		cfg.NetworkSSID = "BAM&DICE";
		cfg.NetworkPassword = "";
		cfg.mode = "AP";
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
	network["mode"] = cfg.mode.c_str();
	network["security"] = cfg.security.c_str();

	char buf[3048];
	root.prettyPrintTo(buf, sizeof(buf));
	fileSetContent(CONFIG_FILE, buf);
}


