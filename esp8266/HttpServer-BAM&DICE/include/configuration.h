#ifndef INCLUDE_CONFIGURATION_H_
#define INCLUDE_CONFIGURATION_H_

#include <user_config.h>
#include <SmingCore/SmingCore.h>

#define CONFIG_FILE ".bam.conf" 

struct BAMConfig
{
	String NetworkSSID;
	String NetworkPassword;
	String mode;
	String security;
	String ip;
};

extern BAMConfig ActiveConfig;

BAMConfig loadConfig();
void saveConfig(BAMConfig& cfg);

#endif
