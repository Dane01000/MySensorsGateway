#include <user_config.h>
#include <SmingCore/SmingCore.h>
#include <AppSettings.h>
#include "Libraries/MySensors/MyGateway.h"
#include "Libraries/MySensors/MyTransport.h"
#include "Libraries/MySensors/MyTransportNRF24.h"
#include "Libraries/MySensors/MyHwESP8266.h"
#include "Libraries/MyInterpreter/MyInterpreter.h"

#define RADIO_CE_PIN        2   // radio chip enable
#define RADIO_SPI_SS_PIN    15  // radio SPI serial select
MyTransportNRF24 transport(RADIO_CE_PIN, RADIO_SPI_SS_PIN, RF24_PA_LEVEL_GW);
MyHwESP8266 hw;
MyGateway gw(transport, hw);

HttpServer server;
FTPServer ftp;

BssList networks;
String network, password;
Timer connectionTimer;

void SendToSensor(String message)
{
    MyMessage msg;
    gw.sendRoute(gw.build(msg, 22, 1, C_REQ, 2, 0));
}

char convBuf[MAX_PAYLOAD*2+1];

MyInterpreter interpreter;

void mqttPublishMessage(String topic, String message);

char V_0[] = "TEMP";		//V_TEMP
char V_1[] = "HUM";		//V_HUM
char V_2[] = "LIGHT";		//V_LIGHT
char V_3[] = "DIMMER";		//V_DIMMER
char V_4[] = "PRESSURE";	//V_PRESSURE
char V_5[] = "FORECAST";	//V_FORECAST
char V_6[] = "RAIN";		//V_RAIN
char V_7[] = "RAINRATE";	//V_RAINRATE
char V_8[] = "WIND";		//V_WIND
char V_9[] = "GUST";		//V_GUST
char V_10[] = "DIRECTON";	//V_DIRECTON
char V_11[] = "UV";		//V_UV
char V_12[] = "WEIGHT";		//V_WEIGHT
char V_13[] = "DISTANCE";	//V_DISTANCE
char V_14[] = "IMPEDANCE";	//V_IMPEDANCE
char V_15[] = "ARMED";		//V_ARMED
char V_16[] = "TRIPPED";	//V_TRIPPED
char V_17[] = "WATT";		//V_WATT
char V_18[] = "KWH";		//V_KWH
char V_19[] = "SCENE_ON";	//V_SCENE_ON
char V_20[] = "SCENE_OFF";	//V_SCENE_OFF
char V_21[] = "HEATER";		//V_HEATER
char V_22[] = "HEATER_SW";	//V_HEATER_SW
char V_23[] = "LIGHT_LEVEL";	//V_LIGHT_LEVEL
char V_24[] = "VAR1";		//V_VAR1
char V_25[] = "VAR2";		//V_VAR2
char V_26[] = "VAR3";		//V_VAR3
char V_27[] = "VAR4";		//V_VAR4
char V_28[] = "VAR5";		//V_VAR5
char V_29[] = "UP";		//V_UP
char V_30[] = "DOWN";		//V_DOWN
char V_31[] = "STOP";		//V_STOP
char V_32[] = "IR_SEND";	//V_IR_SEND
char V_33[] = "IR_RECEIVE";	//V_IR_RECEIVE
char V_34[] = "FLOW";		//V_FLOW
char V_35[] = "VOLUME";		//V_VOLUME
char V_36[] = "LOCK_STATUS";	//V_LOCK_STATUS
char V_37[] = "DUST_LEVEL";	//V_DUST_LEVEL
char V_38[] = "VOLTAGE";	//V_VOLTAGE
char V_39[] = "CURRENT";	//V_CURRENT
char V_40[] = "";		//
char V_41[] = "";		//
char V_42[] = "";		//
char V_43[] = "";		//
char V_44[] = "";		//
char V_45[] = "";		//
char V_46[] = "";		//
char V_47[] = "";		//
char V_48[] = "";		//
char V_49[] = "";		//
char V_50[] = "";		//
char V_51[] = "";		//
char V_52[] = "";		//
char V_53[] = "";		//
char V_54[] = "";		//
char V_55[] = "";		//
char V_56[] = "";		//
char V_57[] = "";		//
char V_58[] = "";		//
char V_59[] = "";		//
char V_60[] = "DEFAULT";	//Custom for MQTTGateway
char V_61[] = "SKETCH_NAME";	//Custom for MQTTGateway
char V_62[] = "SKETCH_VERSION"; //Custom for MQTTGateway
char V_63[] = "UNKNOWN"; 	//Custom for MQTTGateway

//////////////////////////////////////////////////////////////////

const char *vType[] = {
	V_0, V_1, V_2, V_3, V_4, V_5, V_6, V_7, V_8, V_9, V_10,
	V_11, V_12, V_13, V_14, V_15, V_16, V_17, V_18, V_19, V_20,
	V_21, V_22, V_23, V_24, V_25, V_26, V_27, V_28, V_29, V_30,
	V_31, V_32, V_33, V_34, V_35, V_36, V_37, V_38, V_39, V_40,
	V_41, V_42, V_43, V_44, V_45, V_46, V_47, V_48, V_49, V_50,
	V_51, V_52, V_53, V_54, V_55, V_56, V_57, V_58, V_59, V_60,
	V_61, V_62, V_63
};

int getTypeFromString(String type)
{
    for (int x = 0; x < 64; x++)
    {
        if (type.substring(2).equals(vType[x]))
        {
            return x;
        }
    }

    return 0;
}

void incomingMessage(const MyMessage &message)
{
    // Pass along the message from sensors to serial line
    Serial.printf("APP RX %d;%d;%d;%d;%d;%s\n",
                  message.sender, message.sensor,
                  mGetCommand(message), mGetAck(message),
                  message.type, message.getString(convBuf));

    if (mGetCommand(message) == C_SET)
    {
        const char *type = vType[message.type];
        String topic = message.sender + String("/") +
                       message.sensor + String("/") +
                       "V_" + type;
        mqttPublishMessage(topic, message.getString(convBuf));
    }

    interpreter.setVariable('n', message.sender);
    interpreter.setVariable('s', message.sensor);
    interpreter.setVariable('v', atoi(message.getString(convBuf)));

#ifndef DISABLE_SPIFFS
    if (fileExist("rules.script"))
    {
        int size = fileGetSize("rules.script");
        char* progBuf = new char[size + 1];
        fileGetContent("rules.script", progBuf, size + 1);
        interpreter.run(progBuf, strlen(progBuf));
        delete progBuf;
    }
#else
    return;
#endif
}

void onIndex(HttpRequest &request, HttpResponse &response)
{
	TemplateFileStream *tmpl = new TemplateFileStream("index.html");
	auto &vars = tmpl->variables();
	response.sendTemplate(tmpl); // will be automatically deleted
}

void onIpConfig(HttpRequest &request, HttpResponse &response)
{
	if (request.getRequestMethod() == RequestMethod::POST)
	{
		AppSettings.dhcp = request.getPostParameter("dhcp") == "1";
		AppSettings.ip = request.getPostParameter("ip");
		AppSettings.netmask = request.getPostParameter("netmask");
		AppSettings.gateway = request.getPostParameter("gateway");
		debugf("Updating IP settings: %d", AppSettings.ip.isNull());
		AppSettings.save();
	}

	TemplateFileStream *tmpl = new TemplateFileStream("settings.html");
	auto &vars = tmpl->variables();

	bool dhcp = WifiStation.isEnabledDHCP();
	vars["dhcpon"] = dhcp ? "checked='checked'" : "";
	vars["dhcpoff"] = !dhcp ? "checked='checked'" : "";

	if (!WifiStation.getIP().isNull())
	{
		vars["ip"] = WifiStation.getIP().toString();
		vars["netmask"] = WifiStation.getNetworkMask().toString();
		vars["gateway"] = WifiStation.getNetworkGateway().toString();
	}
	else
	{
		vars["ip"] = "192.168.1.77";
		vars["netmask"] = "255.255.255.0";
		vars["gateway"] = "192.168.1.1";
	}

	response.sendTemplate(tmpl); // will be automatically deleted
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

void onAjaxNetworkList(HttpRequest &request, HttpResponse &response)
{
	JsonObjectStream* stream = new JsonObjectStream();
	JsonObject& json = stream->getRoot();

	json["status"] = (bool)true;

	bool connected = WifiStation.isConnected();
	json["connected"] = connected;
	if (connected)
	{
		// Copy full string to JSON buffer memory
		json.addCopy("network", WifiStation.getSSID());
	}

	JsonArray& netlist = json.createNestedArray("available");
	for (int i = 0; i < networks.count(); i++)
	{
		if (networks[i].hidden) continue;
		JsonObject &item = netlist.createNestedObject();
		item.add("id", (int)networks[i].getHashId());
		// Copy full string to JSON buffer memory
		item.addCopy("title", networks[i].ssid);
		item.add("signal", networks[i].rssi);
		item.add("encryption", networks[i].getAuthorizationMethodName());
	}

	response.setAllowCrossDomainOrigin("*");
	response.sendJsonObject(stream);
}

void makeConnection()
{
	WifiStation.enable(true);
	WifiStation.config(network, password);

	AppSettings.ssid = network;
	AppSettings.password = password;
	AppSettings.save();

	network = ""; // task completed
}

void onAjaxConnect(HttpRequest &request, HttpResponse &response)
{
	JsonObjectStream* stream = new JsonObjectStream();
	JsonObject& json = stream->getRoot();

	String curNet = request.getPostParameter("network");
	String curPass = request.getPostParameter("password");

	bool updating = curNet.length() > 0 && (WifiStation.getSSID() != curNet || WifiStation.getPassword() != curPass);
	bool connectingNow = WifiStation.getConnectionStatus() == eSCS_Connecting || network.length() > 0;

	if (updating && connectingNow)
	{
		debugf("wrong action: %s %s, (updating: %d, connectingNow: %d)", network.c_str(), password.c_str(), updating, connectingNow);
		json["status"] = (bool)false;
		json["connected"] = (bool)false;
	}
	else
	{
		json["status"] = (bool)true;
		if (updating)
		{
			network = curNet;
			password = curPass;
			debugf("CONNECT TO: %s %s", network.c_str(), password.c_str());
			json["connected"] = false;
			connectionTimer.initializeMs(1200, makeConnection).startOnce();
		}
		else
		{
			json["connected"] = WifiStation.isConnected();
			debugf("Network already selected. Current status: %s", WifiStation.getConnectionStatusName());
		}
	}

	if (!updating && !connectingNow && WifiStation.isConnectionFailed())
		json["error"] = WifiStation.getConnectionStatusName();

	response.setAllowCrossDomainOrigin("*");
	response.sendJsonObject(stream);
}

extern void checkMqttClient();
void onMqttConfig(HttpRequest &request, HttpResponse &response)
{
    AppSettings.load();
    if (request.getRequestMethod() == RequestMethod::POST)
    {
        AppSettings.mqttUser = request.getPostParameter("user");
        AppSettings.mqttPass = request.getPostParameter("password");
        AppSettings.mqttServer = request.getPostParameter("server");
        AppSettings.mqttPort = atoi(request.getPostParameter("port").c_str());
        AppSettings.save();
        if (WifiStation.isConnected())
            checkMqttClient();
    }

    TemplateFileStream *tmpl = new TemplateFileStream("mqtt.html");
    auto &vars = tmpl->variables();

    vars["user"] = AppSettings.mqttUser;
    vars["password"] = AppSettings.mqttPass;
    vars["server"] = AppSettings.mqttServer;
    vars["port"] = AppSettings.mqttPort;

    response.sendTemplate(tmpl); // will be automatically deleted
}

void startWebServer()
{
	server.listen(80);
	server.addPath("/", onIndex);
	server.addPath("/ipconfig", onIpConfig);
	server.addPath("/mqttconfig", onMqttConfig);
	server.addPath("/ajax/get-networks", onAjaxNetworkList);
	server.addPath("/ajax/connect", onAjaxConnect);
        gw.registerHttpHandlers(server);
	server.setDefaultHandler(onFile);
}

void startFTP()
{
	if (!fileExist("index.html"))
		fileSetContent("index.html", "<h3>Please connect to FTP and upload files from folder 'web/build' (details in code)</h3>");

	// Start FTP server
	ftp.listen(21);
	ftp.addUser("me", "123"); // FTP account
}

int print(int a)
{
  Serial.println(a);
  return a;
}

int updateSensorStateInt(int node, int sensor, int type, int value)
{
  MyMessage myMsg;
  myMsg.set(value);
  gw.sendRoute(gw.build(myMsg, node, sensor, C_SET, 2 /*message.type*/, 0));
}

int updateSensorState(int node, int sensor, int value)
{
  updateSensorStateInt(node, sensor, 2 /*message.type*/, value);
}

// Will be called when system initialization was completed
void startServers()
{
    startFTP();
    startWebServer();

    interpreter.registerFunc1((char *)"print", print);
    interpreter.registerFunc3((char *)"updateSensorState", updateSensorState);

    gw.begin(incomingMessage);
}

void networkScanCompleted(bool succeeded, BssList list)
{
    if (succeeded)
    {
        for (int i = 0; i < list.count(); i++)
            if (!list[i].hidden && list[i].ssid.length() > 0)
                networks.add(list[i]);
    }
    networks.sort([](const BssInfo& a, const BssInfo& b){ return b.rssi - a.rssi; } );
}

Timer connectionCheckTimer;
bool wasConnected = FALSE;
void connectOk();
void connectFail();

void wifiCheckState()
{
    if (WifiStation.isConnected())
    {
        if (!wasConnected)
        {
            Serial.println("CONNECTED");
            wasConnected = TRUE;
            connectOk();
        }
        checkMqttClient();
    }
    else
    {
        if (wasConnected)
        {
            Serial.println("NOT CONNECTED");
            wasConnected = FALSE;
            connectFail();
        }
    }
}

// Will be called when WiFi station was connected to AP
void connectOk()
{
    Serial.println("--> I'm CONNECTED");
}

// Will be called when WiFi station timeout was reached
void connectFail()
{
    Serial.println("--> I'm NOT CONNECTED. Need help :(");
}

void processApplicationCommands(String commandLine, CommandOutput* commandOutput)
{
    Vector<String> commandToken;
    int numToken = splitString(commandLine, ' ' , commandToken);

    if (numToken == 1)
    {
        commandOutput->printf("Example subcommands available : \r\n");
    }
    commandOutput->printf("This command is handled by the application\r\n");
}

void init()
{
    /* Mount the internal storage */
    int slot = rboot_get_current_rom();
#ifndef DISABLE_SPIFFS
    if (slot == 0)
    {
#ifdef RBOOT_SPIFFS_0
        Serial.printf("trying to mount spiffs at %x, length %d\n", RBOOT_SPIFFS_0 + 0x40200000, SPIFF_SIZE);
        spiffs_mount_manual(RBOOT_SPIFFS_0 + 0x40200000, SPIFF_SIZE);
#else
        Serial.printf("trying to mount spiffs at %x, length %d\n", 0x40300000, SPIFF_SIZE);
        spiffs_mount_manual(0x40300000, SPIFF_SIZE);
#endif
    }
    else
    {
#ifdef RBOOT_SPIFFS_1
        Serial.printf("trying to mount spiffs at %x, length %d\n", RBOOT_SPIFFS_1 + 0x40200000, SPIFF_SIZE);
        spiffs_mount_manual(RBOOT_SPIFFS_1 + 0x40200000, SPIFF_SIZE);
#else
        Serial.printf("trying to mount spiffs at %x, length %d\n", 0x40500000, SPIFF_SIZE);
        spiffs_mount_manual(0x40500000, SPIFF_SIZE);
#endif
    }
#else
    Serial.println("spiffs disabled");
#endif

    Serial.begin(SERIAL_BAUD_RATE); // 115200 by default
    Serial.systemDebugOutput(true); // Enable debug output to serial
    //commandHandler.registerCommand(CommandDelegate("example","Example Command from Class","Application",processApplicationCommands));
    //Serial.commandProcessing(true);

    AppSettings.load();

    WifiStation.enable(true);
    //WifiStation.config("WSNatWork", "");
    if (AppSettings.exist())
    {
        WifiStation.config(AppSettings.ssid, AppSettings.password);
        if (!AppSettings.dhcp && !AppSettings.ip.isNull())
            WifiStation.setIP(AppSettings.ip, AppSettings.netmask, AppSettings.gateway);
    }
    //WifiStation.startScan(networkScanCompleted);

    // Start AP for configuration
    WifiAccessPoint.enable(true);
    WifiAccessPoint.config("MySensors gateway", "", AUTH_OPEN);

    wasConnected = FALSE;
    connectionCheckTimer.initializeMs(1000, wifiCheckState).start(true);

    // Run WEB server on system ready
    System.onReady(startServers);
}
