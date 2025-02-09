/*
  The code in this module is only compiled when features are disabled in developer
  mode (ENABLE_DEVELOPER defined).
*/

#ifndef COMPILE_ETHERNET

//----------------------------------------
// Ethernet
//----------------------------------------

void menuEthernet() {systemPrintln("**Ethernet not compiled**");}
void ethernetVerifyTables() {}

void ethernetWebServerStartESP32W5500() {}
void ethernetWebServerStopESP32W5500() {}

bool ntpLogIncreasing = false;

//----------------------------------------
// NTP: Network Time Protocol
//----------------------------------------

void menuNTP() {systemPrint("**NTP not compiled**");}
void ntpServerBegin() {}
void ntpServerUpdate() {}
void ntpValidateTables() {}
void ntpServerStop() {}

#endif // COMPILE_ETHERNET

#ifndef COMPILE_NETWORK

//----------------------------------------
// Network layer
//----------------------------------------

void menuTcpUdp() {systemPrint("**Network not compiled**");}
void networkBegin() {}
IPAddress networkGetIpAddress() {return("0.0.0.0");}
const uint8_t * networkGetMacAddress() 
{
    static const uint8_t zero[6] = {0, 0, 0, 0, 0, 0};
#ifdef COMPILE_BT
    if (bluetoothGetState() != BT_OFF)
        return btMACAddress;
#endif
    return zero;
  }
bool networkIsOnline() {return false;}
void networkMarkOffline(NetIndex_t index) {}
void networkMarkOnline(NetIndex_t index) {}
void networkUpdate() {}
void networkVerifyTables() {}

//----------------------------------------
// NTRIP client
//----------------------------------------

void ntripClientPrintStatus() {systemPrintln("**NTRIP Client not compiled**");}
void ntripClientStop(bool clientAllocated) {online.ntripClient = false;}
void ntripClientUpdate() {}
void ntripClientValidateTables() {}

//----------------------------------------
// NTRIP server
//----------------------------------------

void ntripServerPrintStatus(int serverIndex) {systemPrintf("**NTRIP Server %d not compiled**\r\n", serverIndex);}
void ntripServerProcessRTCM(int serverIndex, uint8_t incoming) {}
void ntripServerStop(int serverIndex, bool clientAllocated) {online.ntripServer[serverIndex] = false;}
void ntripServerUpdate() {}
void ntripServerValidateTables() {}
bool ntripServerIsCasting(int serverIndex) {
    return (false);
}

//----------------------------------------
// TCP client
//----------------------------------------

int32_t tcpClientSendData(uint16_t dataHead) {return 0;}
void tcpClientUpdate() {}
void tcpClientValidateTables() {}
void tcpClientZeroTail() {}
void discardTcpClientBytes(RING_BUFFER_OFFSET previousTail, RING_BUFFER_OFFSET newTail) {}

//----------------------------------------
// TCP server
//----------------------------------------

int32_t tcpServerSendData(uint16_t dataHead) {return 0;}
void tcpServerZeroTail() {}
void tcpServerValidateTables() {}
void discardTcpServerBytes(RING_BUFFER_OFFSET previousTail, RING_BUFFER_OFFSET newTail) {}

//----------------------------------------
// UDP server
//----------------------------------------

int32_t udpServerSendData(uint16_t dataHead) {return 0;}
void udpServerStop() {}
void udpServerUpdate() {}
void udpServerZeroTail() {}
void discardUdpServerBytes(RING_BUFFER_OFFSET previousTail, RING_BUFFER_OFFSET newTail) {}

#endif // COMPILE_NETWORK

//----------------------------------------
// Automatic Over-The-Air (OTA) firmware updates
//----------------------------------------

#ifndef COMPILE_OTA_AUTO

void otaAutoUpdate() {}
void otaUpdateStop() {}
void otaVerifyTables() {}
void otaUpdate() {}

#endif  // COMPILE_OTA_AUTO

//----------------------------------------
// MQTT Client
//----------------------------------------

#ifndef COMPILE_MQTT_CLIENT

bool mqttClientIsConnected() {return false;}
void mqttClientPrintStatus() {}
void mqttClientRestart() {}
void mqttClientUpdate() {}
void mqttClientValidateTables() {}

#endif   // COMPILE_MQTT_CLIENT

//----------------------------------------
// HTTP Client
//----------------------------------------

#ifndef COMPILE_HTTP_CLIENT

void httpClientPrintStatus() {}
void httpClientUpdate() {}
void httpClientValidateTables() {}

#endif   // COMPILE_HTTP_CLIENT

//----------------------------------------
// Web Server
//----------------------------------------

#ifndef COMPILE_AP

bool startWebServer(bool startWiFi = true, int httpPort = 80)
{
    systemPrintln("**AP not compiled**");
    return false;
}
void stopWebServer() {}
bool parseIncomingSettings() {return false;}
void sendStringToWebsocket(const char* stringToSend) {}

#endif  // COMPILE_AP
#ifndef COMPILE_WIFI

//----------------------------------------
// WiFi
//----------------------------------------

void menuWiFi() {systemPrintln("**WiFi not compiled**");}
bool wifiConnect(unsigned long timeout, bool useAPSTAMode, bool *wasInAPmode) {return false;}
int wifiNetworkCount() {return 0;}
bool wifiIsRunning() {return false;}
void wifiRestart() {}
void wifiSetApMode() {}
#define WIFI_STOP() {}

#endif // COMPILE_WIFI

//----------------------------------------
// IM19_IMU
//----------------------------------------

#ifndef  COMPILE_IM19_IMU

void menuTilt() {}
void nmeaApplyCompensation(char *nmeaSentence, int arraySize) {}
void tiltUpdate() {}
void tiltStop() {}
void tiltSensorFactoryReset() {}
bool tiltIsCorrecting() {return(false);}
void tiltRequestStop() {}

#endif  // COMPILE_IM19_IMU

//----------------------------------------
// UM980
//----------------------------------------

#ifndef  COMPILE_UM980

void um980UnicoreHandler(uint8_t * buffer, int length) {}

#endif  // COMPILE_UM980

//----------------------------------------
// mosaic-X5
//----------------------------------------

#ifndef  COMPILE_MOSAICX5

void mosaicVerifyTables() {}
void nmeaExtractStdDeviations(char *nmeaSentence, int arraySize) {}
void processNonSBFData(SEMP_PARSE_STATE *parse) {}
void processUart1SBF(SEMP_PARSE_STATE *parse, uint16_t type) {}
void processUart1SPARTN(SEMP_PARSE_STATE *parse, uint16_t type) {}

#endif  // COMPILE_MOSAICX5

//----------------------------------------
// PointPerfect Library
//----------------------------------------

#ifndef  COMPILE_POINTPERFECT_LIBRARY

void beginPPL() {systemPrintln("**PPL Not Compiled**");}
void updatePPL() {}
bool sendGnssToPpl(uint8_t *buffer, int numDataBytes) {return false;}
bool sendSpartnToPpl(uint8_t *buffer, int numDataBytes) {return false;}
bool sendAuxSpartnToPpl(uint8_t *buffer, int numDataBytes) {return false;}
void pointperfectPrintKeyInformation() {systemPrintln("**PPL Not Compiled**");}

#endif  // COMPILE_POINTPERFECT_LIBRARY

//----------------------------------------
// LG290P
//----------------------------------------

#ifndef COMPILE_LG290P

void lg290pHandler(uint8_t * buffer, int length) {}

#endif // COMPILE_LG290P

//----------------------------------------
// ZED-F9x
//----------------------------------------

#ifndef COMPILE_ZED

// MON HW Antenna Status
enum sfe_ublox_antenna_status_e
{
  SFE_UBLOX_ANTENNA_STATUS_INIT,
  SFE_UBLOX_ANTENNA_STATUS_DONTKNOW,
  SFE_UBLOX_ANTENNA_STATUS_OK,
  SFE_UBLOX_ANTENNA_STATUS_SHORT,
  SFE_UBLOX_ANTENNA_STATUS_OPEN
};

uint8_t aStatus = SFE_UBLOX_ANTENNA_STATUS_DONTKNOW;

// void checkRXMCOR() {}
// void pushRXMPMP() {}
void convertGnssTimeToEpoch(uint32_t *epochSecs, uint32_t *epochMicros) {
    systemPrintln("**Epoch not compiled** ZED not included so time will be invalid");
}

#endif // COMPILE_ZED
