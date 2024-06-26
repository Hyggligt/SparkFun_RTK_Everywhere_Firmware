typedef enum
{
  STATE_ROVER_NOT_STARTED = 0,
  STATE_ROVER_NO_FIX,
  STATE_ROVER_FIX,
  STATE_ROVER_RTK_FLOAT,
  STATE_ROVER_RTK_FIX,
  STATE_BASE_NOT_STARTED,
  STATE_BASE_TEMP_SETTLE, //User has indicated base, but current pos accuracy is too low
  STATE_BASE_TEMP_SURVEY_STARTED,
  STATE_BASE_TEMP_TRANSMITTING,
  STATE_BASE_FIXED_NOT_STARTED,
  STATE_BASE_FIXED_TRANSMITTING,
  STATE_BUBBLE_LEVEL,
  STATE_MARK_EVENT,
  STATE_DISPLAY_SETUP,
  STATE_WIFI_CONFIG_NOT_STARTED,
  STATE_WIFI_CONFIG,
  STATE_TEST,
  STATE_TESTING,
  STATE_PROFILE,
  STATE_KEYS_STARTED,
  STATE_KEYS_NEEDED,
  STATE_KEYS_WIFI_STARTED,
  STATE_KEYS_WIFI_CONNECTED,
  STATE_KEYS_WIFI_TIMEOUT,
  STATE_KEYS_EXPIRED,
  STATE_KEYS_DAYS_REMAINING,
  STATE_KEYS_LBAND_CONFIGURE,
  STATE_KEYS_LBAND_ENCRYPTED,
  STATE_KEYS_PROVISION_STARTED,
  STATE_KEYS_PROVISION_CONNECTED,
  STATE_KEYS_PROVISION_WIFI_TIMEOUT,
  STATE_ESPNOW_PAIRING_NOT_STARTED,
  STATE_ESPNOW_PAIRING,
  STATE_NTPSERVER_NOT_STARTED,
  STATE_NTPSERVER_NO_SYNC,
  STATE_NTPSERVER_SYNC,
  STATE_CONFIG_VIA_ETH_NOT_STARTED,
  STATE_CONFIG_VIA_ETH_STARTED,
  STATE_CONFIG_VIA_ETH,
  STATE_CONFIG_VIA_ETH_RESTART_BASE,
  STATE_SHUTDOWN,
  STATE_NOT_SET, //Must be last on list
} SystemState;

typedef enum
{
  ETH_NOT_STARTED,
  ETH_STARTED_CHECK_CABLE,
  ETH_STARTED_START_DHCP,
  ETH_CONNECTED,
  ETH_CAN_NOT_BEGIN,
} ethernetStatus_e;

typedef enum LoggingType {
  LOGGING_UNKNOWN = 0,
  LOGGING_STANDARD,
  LOGGING_PPP,
  LOGGING_CUSTOM
} LoggingType;
LoggingType loggingType = LOGGING_STANDARD;

typedef enum
{
  NTRIP_SERVER_OFF = 0,         //Using Bluetooth or NTRIP client
  NTRIP_SERVER_ON,              //WIFI_START state
  NTRIP_SERVER_WIFI_ETHERNET_STARTED,   //Connecting to WiFi access point
  NTRIP_SERVER_WIFI_ETHERNET_CONNECTED, //WiFi connected to an access point
  NTRIP_SERVER_WAIT_GNSS_DATA,  //Waiting for correction data from GNSS
  NTRIP_SERVER_CONNECTING,      //Attempting a connection to the NTRIP caster
  NTRIP_SERVER_AUTHORIZATION,   //Validate the credentials
  NTRIP_SERVER_CASTING,         //Sending correction data to the NTRIP caster
} NTRIPServerState;
NTRIPServerState ntripServerState = NTRIP_SERVER_CASTING;

typedef enum
{
  RTK_SURVEYOR = 0,
  RTK_EXPRESS,
  RTK_FACET,
  RTK_EXPRESS_PLUS,
  RTK_FACET_LBAND,
  REFERENCE_STATION,
  RTK_UNKNOWN,
} ProductVariant;
ProductVariant productVariant = REFERENCE_STATION;
