/*
 * AppSettings.h
 *
 *  Created on: 13 ��� 2015 �.
 *      Author: Anakod
 */

#include <SmingCore/SmingCore.h>

#ifndef INCLUDE_APPSETTINGS_H_
#define INCLUDE_APPSETTINGS_H_

#define APP_SETTINGS_FILE ".settings.conf" // leading point for security reasons :)

class ApplicationSettingsStorage
{
  public:
    String ssid;
    String password;
    String apPassword;

    String portalUrl;
    String portalData;

    bool dhcp = true;

    IPAddress ip;
    IPAddress netmask;
    IPAddress gateway;

    String mqttUser;
    String mqttPass;
    String mqttServer;
    int    mqttPort;
    String mqttSensorPfx;
    String mqttControllerPfx;

    bool   cpuBoost = true;
    bool   useOwnBaseAddress = true;

    String cloudDeviceToken;
    String cloudLogin;
    String cloudPassword;

    void load();
    void save();

    bool exist() { return fileExist(APP_SETTINGS_FILE); }
};

extern ApplicationSettingsStorage AppSettings;

#endif /* INCLUDE_APPSETTINGS_H_ */
