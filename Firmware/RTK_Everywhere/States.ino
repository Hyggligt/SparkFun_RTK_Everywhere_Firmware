/*
  This is the main state machine for the device. It's big but controls each step of the system.
  See system state diagram for a visual representation of how states can change to/from.
  Statemachine diagram:
  https://lucid.app/lucidchart/53519501-9fa5-4352-aa40-673f88ca0c9b/edit?invitationId=inv_ebd4b988-513d-4169-93fd-c291851108f8
*/

static uint32_t lastStateTime = 0;

// Given the current state, see if conditions have moved us to a new state
// A user pressing the mode button (change between rover/base) is handled by buttonCheckTask()
void stateUpdate()
{
    if (millis() - lastSystemStateUpdate > 500 || forceSystemStateUpdate == true)
    {
        lastSystemStateUpdate = millis();
        forceSystemStateUpdate = false;

        // Check to see if any external sources need to change state
        if (newSystemStateRequested == true)
        {
            newSystemStateRequested = false;
            if (systemState != requestedSystemState)
            {
                changeState(requestedSystemState);
                lastStateTime = millis();
            }
        }

        if (settings.enablePrintStates && ((millis() - lastStateTime) > 15000))
        {
            changeState(systemState);
            lastStateTime = millis();
        }

        // Move between states as needed
        DMW_st(changeState, systemState);
        switch (systemState)
        {
        /*
                          .-----------------------------------.
                          |      STATE_ROVER_NOT_STARTED      |
                          | Text: 'Rover' and 'Rover Started' |
                          '-----------------------------------'
                                            |
                                            |
                                            |
                                            V
                          .-----------------------------------.
                          |         STATE_ROVER_NO_FIX        |
                          |           SIV Icon Blink          |
                          |            "HPA: >30m"            |
                          |             "SIV: 0"              |
                          '-----------------------------------'
                                            |
                                            | GPS Lock
                                            | 3D, 3D+DR
                                            V
                          .-----------------------------------.
                          |          STATE_ROVER_FIX          | Carrier
                          |           SIV Icon Solid          | Solution = 2
                .-------->|            "HPA: .513"            |---------.
                |         |             "SIV: 30"             |         |
                |         '-----------------------------------'         |
                |                           |                           |
                |                           | Carrier Solution = 1      |
                |                           V                           |
                |         .-----------------------------------.         |
                |         |       STATE_ROVER_RTK_FLOAT       |         |
                |  No RTK |     Double Crosshair Blinking     |         |
                +<--------|           "*HPA: .080"            |         |
                ^         |             "SIV: 30"             |         |
                |         '-----------------------------------'         |
                |                        ^         |                    |
                |                        |         | Carrier            |
                |                        |         | Solution = 2       |
                |                        |         V                    |
                |                Carrier |         +<-------------------'
                |           Solution = 1 |         |
                |                        |         V
                |         .-----------------------------------.
                |         |        STATE_ROVER_RTK_FIX        |
                |  No RTK |       Double Crosshair Solid      |
                '---------|           "*HPA: .014"            |
                          |             "SIV: 30"             |
                          '-----------------------------------'

        */
        case (STATE_ROVER_NOT_STARTED): {
            RTK_MODE(RTK_MODE_ROVER);
            if (online.gnss == false)
            {
                firstRoverStart = false; // If GNSS is offline, we still need to allow button use
                return;
            }

            baseStatusLedOff();

            // Configure for rover mode
            displayRoverStart(0);
            if (gnssConfigureRover() == false)
            {
                systemPrintln("Rover config failed");
                displayRoverFail(1000);
                return;
            }

            setMuxport(settings.dataPortChannel); // Return mux to original channel

            bluetoothStart(); // Turn on Bluetooth with 'Rover' name
            espnowStart();     // Start internal radio if enabled, otherwise disable

            // Start the UART connected to the GNSS receiver for NMEA and UBX data (enables logging)
            if (tasksStartGnssUart() == false)
                displayRoverFail(1000);
            else
            {
                settings.updateGNSSSettings = false; // On the next boot, no need to update the ZED on this profile
                settings.lastState = STATE_ROVER_NOT_STARTED;
                recordSystemSettings(); // Record this state for next POR

                displayRoverSuccess(500);

                changeState(STATE_ROVER_NO_FIX);

                firstRoverStart = false; // Do not allow entry into test menu again
            }
        }
        break;

        case (STATE_ROVER_NO_FIX): {
            if (gnssIsFixed()) // 3D, 3D+DR
                changeState(STATE_ROVER_FIX);
        }
        break;

        case (STATE_ROVER_FIX): {
            if (gnssIsRTKFloat())
                changeState(STATE_ROVER_RTK_FLOAT);
            else if (gnssIsRTKFix())
                changeState(STATE_ROVER_RTK_FIX);
        }
        break;

        case (STATE_ROVER_RTK_FLOAT): {
            if (gnssIsRTKFix() == false && gnssIsRTKFloat() == false) // No RTK
                changeState(STATE_ROVER_FIX);
            if (gnssIsRTKFix() == true)
                changeState(STATE_ROVER_RTK_FIX);
        }
        break;

        case (STATE_ROVER_RTK_FIX): {
            if (gnssIsRTKFix() == false && gnssIsRTKFloat() == false) // No RTK
                changeState(STATE_ROVER_FIX);
            if (gnssIsRTKFloat())
                changeState(STATE_ROVER_RTK_FLOAT);
        }
        break;

            /*
                              .-----------------------------------.
                  startBase() |      STATE_BASE_NOT_STARTED       |
                 .------------|            Text: 'Base'           |
                 |    = false '-----------------------------------'
                 |                              |
                 |  Stop WiFi,                  | startBase() = true
                 |      Stop                    | Stop WiFi
                 |  Bluetooth                   | Start Bluetooth
                 |                              V
                 |            .-----------------------------------.
                 |            |      STATE_BASE_TEMP_SETTLE       |
                 |            |   Temp Base Icon. Blinking HPA.   |
                 |            |           "HPA: 7.15"             |
                 |            |             "SIV: 5"              |
                 |            '-----------------------------------'
                 V                              |
              STATE_BASE_FIXED_NOT_STARTED      | gnssGetHorizontalAccuracy() > 0.0
              (next diagram)                    | && gnssGetHorizontalAccuracy()
                                                |  < settings.surveyInStartingAccuracy
                                                | && gnssSurveyInStart() == true
                                                V
                              .-----------------------------------.
                              |   STATE_BASE_TEMP_SURVEY_STARTED  | svinObservationTime >
                              |       Temp Base Icon blinking     | maxSurveyInWait_s
                              |           "Mean: 0.089"           |--------------.
                              |            "Time: 36"             |              |
                              '-----------------------------------'              |
                                                |                                |
                                                | getSurveyInValid()             |
                                                | = true                         V
                                                |              STATE_ROVER_NOT_STARTED
                                                V                   (Previous diagram)
                              .-----------------------------------.
                              |    STATE_BASE_TEMP_TRANSMITTING   |
                              |        Temp Base Icon solid       |
                              |             "Xmitting"            |
                              |            "RTCM: 2145"           |
                              '-----------------------------------'

            */

        case (STATE_BASE_NOT_STARTED): {
            RTK_MODE(RTK_MODE_BASE_SURVEY_IN);
            firstRoverStart = false; // If base is starting, no test menu, normal button use.

            if (online.gnss == false)
                return;

            baseStatusLedOff();

            displayBaseStart(0); // Show 'Base'

            bluetoothStop();
            bluetoothStart(); // Restart Bluetooth with 'Base' identifier

            // Start the UART connected to the GNSS receiver for NMEA and UBX data (enables logging)
            if (tasksStartGnssUart() && gnssConfigureBase())
            {
                settings.updateGNSSSettings = false; // On the next boot, no need to update the GNSS on this profile
                settings.lastState = STATE_BASE_NOT_STARTED; // Record this state for next POR
                recordSystemSettings();                      // Record this state for next POR

                displayBaseSuccess(500); // Show 'Base Started'

                if (settings.fixedBase == false)
                    changeState(STATE_BASE_TEMP_SETTLE);
                else if (settings.fixedBase == true)
                    changeState(STATE_BASE_FIXED_NOT_STARTED);
            }
            else
            {
                displayBaseFail(1000);
            }
        }
        break;

        // Wait for horz acc of 5m or less before starting survey in
        case (STATE_BASE_TEMP_SETTLE): {
            // Blink base LED slowly while we wait for first fix
            if (millis() - lastBaseLEDupdate > 1000)
            {
                lastBaseLEDupdate = millis();

                baseStatusLedBlink(); // Toggle the base/status LED
            }

            int siv = gnssGetSatellitesInView();
            float hpa = gnssGetHorizontalAccuracy();

            // Check for <1m horz accuracy before starting surveyIn
            char accuracy[20];
            char temp[20];
            const char *units = getHpaUnits(hpa, temp, sizeof(temp), 2, true);
            // gnssGetSurveyInStartingAccuracy is 10m max
            const char *accUnits = getHpaUnits(gnssGetSurveyInStartingAccuracy(), accuracy, sizeof(accuracy), 2, false);
            systemPrintf("Waiting for Horz Accuracy < %s (%s): %s%s%s%s, SIV: %d\r\n", accuracy, accUnits, temp,
                         (accUnits != units) ? " (" : "", (accUnits != units) ? units : "",
                         (accUnits != units) ? ")" : "", siv);

            // On the mosaic-X5, the HPA is undefined while the GNSS is determining its fixed position
            // We need to skip the HPA check...
            if ((hpa > 0.0 && hpa < gnssGetSurveyInStartingAccuracy()) || present.gnss_mosaicX5)
            {
                displaySurveyStart(0); // Show 'Survey'

                if (gnssSurveyInStart() == true) // Begin survey
                {
                    displaySurveyStarted(500); // Show 'Survey Started'

                    changeState(STATE_BASE_TEMP_SURVEY_STARTED);
                }
            }
        }
        break;

        // Check survey status until it completes or 15 minutes elapses and we go back to rover
        case (STATE_BASE_TEMP_SURVEY_STARTED): {
            // Blink base LED quickly during survey in
            if (millis() - lastBaseLEDupdate > 500)
            {
                lastBaseLEDupdate = millis();

                baseStatusLedBlink(); // Toggle the base/status LED
            }

            // Get the data once to avoid duplicate slow responses
            int observationTime = gnssGetSurveyInObservationTime();
            float meanAccuracy = gnssGetSurveyInMeanAccuracy();
            int siv = gnssGetSatellitesInView();

            if (gnssIsSurveyComplete() == true) // Survey in complete
            {
                systemPrintf("Observation Time: %d\r\n", observationTime);
                systemPrintln("Base survey complete! RTCM now broadcasting.");

                baseStatusLedOn(); // Indicate survey complete

                // Start the NTRIP server if requested
                RTK_MODE(RTK_MODE_BASE_FIXED);

                espnowStart(); // Start internal radio if enabled, otherwise disable

                rtcmPacketsSent = 0; // Reset any previous number
                changeState(STATE_BASE_TEMP_TRANSMITTING);
            }
            else
            {
                char temp[20];
                const char *units = getHpaUnits(meanAccuracy, temp, sizeof(temp), 3, true);
                systemPrintf("Time elapsed: %d Accuracy (%s): %s SIV: %d\r\n", observationTime, units, temp, siv);

                if (observationTime > maxSurveyInWait_s)
                {
                    systemPrintf("Survey-In took more than %d minutes. Returning to rover mode.\r\n",
                                 maxSurveyInWait_s / 60);

                    if (gnssSurveyInReset() == false)
                    {
                        systemPrintln("Survey reset failed - attempt 1/3");
                        if (gnssSurveyInReset() == false)
                        {
                            systemPrintln("Survey reset failed - attempt 2/3");
                            if (gnssSurveyInReset() == false)
                            {
                                systemPrintln("Survey reset failed - attempt 3/3");
                            }
                        }
                    }

                    changeState(STATE_ROVER_NOT_STARTED);
                }
            }
        }
        break;

        // Leave base temp transmitting over external radio, or WiFi/NTRIP, or ESP NOW
        case (STATE_BASE_TEMP_TRANSMITTING): {
        }
        break;

        /*
                          .-----------------------------------.
              startBase() |   STATE_BASE_FIXED_NOT_STARTED    |
                  = false |        Text: "Base Started"       |
            .-------------|                                   |
            |             '-----------------------------------'
            V                               |
          STATE_ROVER_NOT_STARTED             | startBase() = true
          (Rover diagram)                     V
                          .-----------------------------------.
                          |   STATE_BASE_FIXED_TRANSMITTING   |
                          |       Castle Base Icon solid      |
                          |            "Xmitting"             |
                          |            "RTCM: 0"              |
                          '-----------------------------------'

        */

        // User has switched to base with fixed option enabled. Let's configure and try to get there.
        // If fixed base fails, we'll handle it here
        case (STATE_BASE_FIXED_NOT_STARTED): {
            RTK_MODE(RTK_MODE_BASE_FIXED);
            bool response = gnssFixedBaseStart();
            if (response == true)
            {
                baseStatusLedOn(); // Turn on the base/status LED

                espnowStart(); // Start internal radio if enabled, otherwise disable

                changeState(STATE_BASE_FIXED_TRANSMITTING);
            }
            else
            {
                systemPrintln("Fixed base start failed");
                displayBaseFail(1000);

                changeState(STATE_ROVER_NOT_STARTED); // Return to rover mode to avoid being in fixed base mode
            }
        }
        break;

        // Leave base fixed transmitting if user has enabled WiFi/NTRIP
        case (STATE_BASE_FIXED_TRANSMITTING): {
        }
        break;

        case (STATE_DISPLAY_SETUP): {
            if (millis() - lastSetupMenuChange > 10000) // Exit Setup after 10s
            {
                // forceSystemStateUpdate = true; // Immediately go to this new state
                changeState(lastSystemState); // Return to the last system state
            }
        }
        break;

        case (STATE_WIFI_CONFIG_NOT_STARTED): {
            if (pin_bluetoothStatusLED != PIN_UNDEFINED)
            {
                // Start BT LED Fade to indicate the start of WiFi
                bluetoothLedTask.detach(); // Increase BT LED blinker task rate
                bluetoothLedTask.attach(bluetoothLedTaskPace33Hz,
                                        tickerBluetoothLedUpdate); // Rate in seconds, callback
            }

            baseStatusLedOff(); // Turn off the status LED

            displayWiFiConfigNotStarted(); // Display immediately during SD cluster pause

            WIFI_STOP(); // Notify the network layer that it should stop so we can take over control of WiFi
            bluetoothStop();
            espnowStop();

            tasksStopGnssUart();   // Delete serial tasks if running
            if (!startWebServer()) // Start web server in WiFi mode and show config html page
                changeState(STATE_ROVER_NOT_STARTED);
            else
            {
                RTK_MODE(RTK_MODE_WIFI_CONFIG);
                changeState(STATE_WIFI_CONFIG);
            }
        }
        break;

        case (STATE_WIFI_CONFIG): {
            if (incomingSettingsSpot > 0)
            {
                // Allow for 750ms before we parse buffer for all data to arrive
                if (millis() - timeSinceLastIncomingSetting > 750)
                {
                    currentlyParsingData =
                        true; // Disallow new data to flow from websocket while we are parsing the current data

                    systemPrint("Parsing: ");
                    for (int x = 0; x < incomingSettingsSpot; x++)
                        systemWrite(incomingSettings[x]);
                    systemPrintln();

                    parseIncomingSettings();
                    settings.updateGNSSSettings =
                        true;               // When this profile is loaded next, force system to update GNSS settings.
                    recordSystemSettings(); // Record these settings to unit

                    // Clear buffer
                    incomingSettingsSpot = 0;
                    memset(incomingSettings, 0, AP_CONFIG_SETTING_SIZE);

                    currentlyParsingData = false; // Allow new data from websocket
                }
            }

#ifdef COMPILE_WIFI
#ifdef COMPILE_AP
            // Dynamically update the coordinates on the AP page
            if (websocketConnected == true)
            {
                if (millis() - lastDynamicDataUpdate > 1000)
                {
                    lastDynamicDataUpdate = millis();
                    createDynamicDataString(settingsCSV);

                    // log_d("Sending coordinates: %s", settingsCSV);
                    sendStringToWebsocket(settingsCSV);
                }
            }
#endif // COMPILE_AP
#endif // COMPILE_WIFI
        }
        break;

        // Setup device for testing
        case (STATE_TEST): {
            // Debounce entry into test menu
            if (millis() - lastTestMenuChange > 500)
            {
                tasksStopGnssUart(); // Stop absoring GNSS serial via task
                zedUartPassed = false;

                gnssEnableRTCMTest();

                RTK_MODE(RTK_MODE_TESTING);
                changeState(STATE_TESTING);
            }
        }
        break;

        // Display testing screen - do nothing
        case (STATE_TESTING): {
            // Exit via button press task
        }
        break;

        case (STATE_PROFILE): {
            // Do nothing - display only
        }
        break;

        case (STATE_KEYS_REQUESTED): {
            settings.requestKeyUpdate = true; // Force a key update
            changeState(lastSystemState); // Return to the last system state
        }
        break;

        case (STATE_ESPNOW_PAIRING_NOT_STARTED): {
#ifdef COMPILE_ESPNOW
            paintEspNowPairing();

            // Start ESP-Now if needed, put ESP-Now into broadcast state
            espnowBeginPairing();

            changeState(STATE_ESPNOW_PAIRING);
#else  // COMPILE_ESPNOW
            changeState(STATE_ROVER_NOT_STARTED);
#endif // COMPILE_ESPNOW
        }
        break;

        case (STATE_ESPNOW_PAIRING): {
            if (espnowIsPaired() == true)
            {
                paintEspNowPaired();

                // Return to the previous state
                changeState(lastSystemState);
            }
            else
            {
                uint8_t broadcastMac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
                espnowSendPairMessage(broadcastMac); // Send unit's MAC address over broadcast, no ack, no encryption
            }
        }
        break;

#ifdef COMPILE_ETHERNET
        case (STATE_NTPSERVER_NOT_STARTED): {
            RTK_MODE(RTK_MODE_NTP);
            firstRoverStart = false; // If NTP is starting, no test menu, normal button use.

            if (online.gnss == false)
                return;

            displayNtpStart(500); // Show 'NTP'

            // Start UART connected to the GNSS receiver for NMEA and UBX data (enables logging)
            if (tasksStartGnssUart() && configureUbloxModuleNTP())
            {
                settings.updateGNSSSettings = false; // On the next boot, no need to update the GNSS on this profile
                settings.lastState = STATE_NTPSERVER_NOT_STARTED; // Record this state for next POR
                recordSystemSettings();

                if (online.ethernetNTPServer)
                {
                    if (settings.debugNtp)
                        systemPrintln("NTP Server started");
                    displayNtpStarted(500); // Show 'NTP Started'
                    changeState(STATE_NTPSERVER_NO_SYNC);
                }
                else
                {
                    if (settings.debugNtp)
                        systemPrintln("NTP Server waiting for Ethernet");
                    displayNtpNotReady(1000); // Show 'Ethernet Not Ready'
                    changeState(STATE_NTPSERVER_NO_SYNC);
                }
            }
            else
            {
                if (settings.debugNtp)
                    systemPrintln("NTP Server ZED configuration failed");
                displayNTPFail(1000); // Show 'NTP Failed'
                // Do we stay in STATE_NTPSERVER_NOT_STARTED? Or should we reset?
            }
        }
        break;

        case (STATE_NTPSERVER_NO_SYNC): {
            if (rtcSyncd)
            {
                if (settings.debugNtp)
                    systemPrintln("NTP Server RTC synchronized");
                changeState(STATE_NTPSERVER_SYNC);
            }
        }
        break;

        case (STATE_NTPSERVER_SYNC): {
            if (!rtcSyncd)
            {
                if (settings.debugNtp)
                    systemPrintln("NTP Server RTC sync lost");
                changeState(STATE_NTPSERVER_NO_SYNC);
            }
        }
        break;

        case (STATE_CONFIG_VIA_ETH_NOT_STARTED): {
            displayConfigViaEthStarting(1500);

            settings.updateGNSSSettings = false; // On the next boot, no need to update the GNSS on this profile
            settings.lastState = STATE_CONFIG_VIA_ETH_STARTED; // Record the _next_ state for POR
            recordSystemSettings();

            forceConfigureViaEthernet(); // Create a file in LittleFS to force code into configure-via-ethernet mode

            ESP.restart(); // Restart to go into the dedicated configure-via-ethernet mode
        }
        break;

        case (STATE_CONFIG_VIA_ETH_STARTED): {
            RTK_MODE(RTK_MODE_ETHERNET_CONFIG);
            // The code should only be able to enter this state if configureViaEthernet is true.
            // If configureViaEthernet is not true, we need to restart again.
            if (!configureViaEthernet)
            {
                displayConfigViaEthStarting(1500);
                settings.lastState = STATE_CONFIG_VIA_ETH_STARTED; // Re-record this state for POR
                recordSystemSettings();

                forceConfigureViaEthernet(); // Create a file in LittleFS to force code into configure-via-ethernet mode

                ESP.restart(); // Restart to go into the dedicated configure-via-ethernet mode
            }

            displayConfigViaEthStarted(1500);

            bluetoothStop();     // Should be redundant - but just in case
            espnowStop();        // Should be redundant - but just in case
            tasksStopGnssUart(); // Delete F9 serial tasks if running

            ethernetWebServerStartESP32W5500(); // Start Ethernet in dedicated configure-via-ethernet mode

            if (!startWebServer(false, settings.httpPort)) // Start the web server
                changeState(STATE_ROVER_NOT_STARTED);
            else
                changeState(STATE_CONFIG_VIA_ETH);
        }
        break;

        case (STATE_CONFIG_VIA_ETH): {
            // Display will show the IP address (displayConfigViaEthernet)

            if (incomingSettingsSpot > 0)
            {
                // Allow for 750ms before we parse buffer for all data to arrive
                if (millis() - timeSinceLastIncomingSetting > 750)
                {
                    currentlyParsingData =
                        true; // Disallow new data to flow from websocket while we are parsing the current data

                    systemPrint("Parsing: ");
                    for (int x = 0; x < incomingSettingsSpot; x++)
                        systemWrite(incomingSettings[x]);
                    systemPrintln();

                    parseIncomingSettings();
                    settings.updateGNSSSettings =
                        true;               // When this profile is loaded next, force system to update GNSS settings.
                    recordSystemSettings(); // Record these settings to unit

                    // Clear buffer
                    incomingSettingsSpot = 0;
                    memset(incomingSettings, 0, AP_CONFIG_SETTING_SIZE);

                    currentlyParsingData = false; // Allow new data from websocket
                }
            }

#ifdef COMPILE_WIFI
#ifdef COMPILE_AP
            // Dynamically update the coordinates on the AP page
            if (websocketConnected == true)
            {
                if (millis() - lastDynamicDataUpdate > 1000)
                {
                    lastDynamicDataUpdate = millis();
                    createDynamicDataString(settingsCSV);

                    // log_d("Sending coordinates: %s", settingsCSV);
                    sendStringToWebsocket(settingsCSV);
                }
            }
#endif // COMPILE_AP
#endif // COMPILE_WIFI
        }
        break;

        case (STATE_CONFIG_VIA_ETH_RESTART_BASE): {
            displayConfigViaEthExiting(1000);

            ethernetWebServerStopESP32W5500();

            settings.updateGNSSSettings = false;         // On the next boot, no need to update the GNSS on this profile
            settings.lastState = STATE_BASE_NOT_STARTED; // Record the _next_ state for POR
            recordSystemSettings();

            ESP.restart();
        }
        break;
#endif // COMPILE_ETHERNET

        case (STATE_SHUTDOWN): {
            forceDisplayUpdate = true;
            powerDown(true);
        }
        break;

        default: {
            systemPrintf("Unknown state: %d\r\n", systemState);
        }
        break;
        }
    }
}

// System state changes may only occur within main state machine
// To allow state changes from external sources (ie, Button Tasks) requests can be made
// Requests are handled at the start of stateUpdate()
void requestChangeState(SystemState requestedState)
{
    newSystemStateRequested = true;
    requestedSystemState = requestedState;
    log_d("Requested System State: %d", requestedSystemState);
}

// Print the current state
const char *getState(SystemState state, char *buffer)
{
    switch (state)
    {
    case (STATE_ROVER_NOT_STARTED):
        return "STATE_ROVER_NOT_STARTED";
    case (STATE_ROVER_NO_FIX):
        return "STATE_ROVER_NO_FIX";
    case (STATE_ROVER_FIX):
        return "STATE_ROVER_FIX";
    case (STATE_ROVER_RTK_FLOAT):
        return "STATE_ROVER_RTK_FLOAT";
    case (STATE_ROVER_RTK_FIX):
        return "STATE_ROVER_RTK_FIX";
    case (STATE_BASE_NOT_STARTED):
        return "STATE_BASE_NOT_STARTED";
    case (STATE_BASE_TEMP_SETTLE):
        return "STATE_BASE_TEMP_SETTLE";
    case (STATE_BASE_TEMP_SURVEY_STARTED):
        return "STATE_BASE_TEMP_SURVEY_STARTED";
    case (STATE_BASE_TEMP_TRANSMITTING):
        return "STATE_BASE_TEMP_TRANSMITTING";
    case (STATE_BASE_FIXED_NOT_STARTED):
        return "STATE_BASE_FIXED_NOT_STARTED";
    case (STATE_BASE_FIXED_TRANSMITTING):
        return "STATE_BASE_FIXED_TRANSMITTING";
    case (STATE_DISPLAY_SETUP):
        return "STATE_DISPLAY_SETUP";
    case (STATE_WIFI_CONFIG_NOT_STARTED):
        return "STATE_WIFI_CONFIG_NOT_STARTED";
    case (STATE_WIFI_CONFIG):
        return "STATE_WIFI_CONFIG";
    case (STATE_TEST):
        return "STATE_TEST";
    case (STATE_TESTING):
        return "STATE_TESTING";
    case (STATE_PROFILE):
        return "STATE_PROFILE";

    case (STATE_KEYS_REQUESTED):
        return "STATE_KEYS_REQUESTED";

    case (STATE_ESPNOW_PAIRING_NOT_STARTED):
        return "STATE_ESPNOW_PAIRING_NOT_STARTED";
    case (STATE_ESPNOW_PAIRING):
        return "STATE_ESPNOW_PAIRING";

    case (STATE_NTPSERVER_NOT_STARTED):
        return "STATE_NTPSERVER_NOT_STARTED";
    case (STATE_NTPSERVER_NO_SYNC):
        return "STATE_NTPSERVER_NO_SYNC";
    case (STATE_NTPSERVER_SYNC):
        return "STATE_NTPSERVER_SYNC";

    case (STATE_CONFIG_VIA_ETH_NOT_STARTED):
        return "STATE_CONFIG_VIA_ETH_NOT_STARTED";
    case (STATE_CONFIG_VIA_ETH_STARTED):
        return "STATE_CONFIG_VIA_ETH_STARTED";
    case (STATE_CONFIG_VIA_ETH):
        return "STATE_CONFIG_VIA_ETH";
    case (STATE_CONFIG_VIA_ETH_RESTART_BASE):
        return "STATE_CONFIG_VIA_ETH_RESTART_BASE";

    case (STATE_SHUTDOWN):
        return "STATE_SHUTDOWN";
    case (STATE_NOT_SET):
        return "STATE_NOT_SET";
    }

    // Handle the unknown case
    sprintf(buffer, "Unknown: %d", state);
    return buffer;
}

// Change states and print the new state
void changeState(SystemState newState)
{
    char string1[30];
    char string2[30];
    const char *arrow = "";
    const char *asterisk = "";
    const char *initialState = "";
    const char *endingState = "";

    // Log the heap size at the state change
    reportHeapNow(false);

    // Debug print of new state, add leading asterisk for repeated states
    if ((!settings.enablePrintDuplicateStates) && (newState == systemState))
        return;

    if (settings.enablePrintStates)
    {
        arrow = "";
        asterisk = "";
        initialState = "";
        if (newState == systemState)
            asterisk = "*";
        else
        {
            initialState = getState(systemState, string1);
            arrow = " --> ";
        }
    }

    // Set the new state
    systemState = newState;
    if (settings.enablePrintStates)
    {
        endingState = getState(newState, string2);

        if (!online.rtc)
            systemPrintf("%s%s%s%s\r\n", asterisk, initialState, arrow, endingState);
        else
        {
            // Timestamp the state change
            //          1         2
            // 12345678901234567890123456
            // YYYY-mm-dd HH:MM:SS.xxxrn0
            struct tm timeinfo = rtc.getTimeStruct();
            char s[30];
            strftime(s, sizeof(s), "%Y-%m-%d %H:%M:%S", &timeinfo);
            systemPrintf("%s%s%s%s, %s.%03ld\r\n", asterisk, initialState, arrow, endingState, s, rtc.getMillis());
        }
    }
}

// RTK mode structure
typedef struct _RTK_MODE_ENTRY
{
    const char *modeName;
    SystemState first;
    SystemState last;
} RTK_MODE_ENTRY;

const RTK_MODE_ENTRY stateModeTable[] = {
    {"Rover", STATE_ROVER_NOT_STARTED, STATE_ROVER_RTK_FIX},
    {"Base", STATE_BASE_NOT_STARTED, STATE_BASE_FIXED_TRANSMITTING},
    {"Setup", STATE_DISPLAY_SETUP, STATE_PROFILE},
    {"ESPNOW Pairing", STATE_ESPNOW_PAIRING_NOT_STARTED, STATE_ESPNOW_PAIRING},
    {"NTP", STATE_NTPSERVER_NOT_STARTED, STATE_NTPSERVER_SYNC},
    {"Ethernet Config", STATE_CONFIG_VIA_ETH_NOT_STARTED, STATE_CONFIG_VIA_ETH_RESTART_BASE},
    {"Shutdown", STATE_SHUTDOWN, STATE_SHUTDOWN}};
const int stateModeTableEntries = sizeof(stateModeTable) / sizeof(stateModeTable[0]);

const char *stateToRtkMode(SystemState state)
{
    const RTK_MODE_ENTRY *mode;

    // Walk the RTK mode table
    for (int index = 0; index < stateModeTableEntries; index++)
    {
        mode = &stateModeTable[index];
        if ((state >= mode->first) && (state <= mode->last))
            return mode->modeName;
    }

    // Unknown mode
    return "Unknown Mode";
}

bool inRoverMode()
{
    if (systemState >= STATE_ROVER_NOT_STARTED && systemState <= STATE_ROVER_RTK_FIX)
        return (true);
    return (false);
}

bool inBaseMode()
{
    if (systemState >= STATE_BASE_NOT_STARTED && systemState <= STATE_BASE_FIXED_TRANSMITTING)
        return (true);
    return (false);
}

bool inWiFiConfigMode()
{
    if (systemState >= STATE_WIFI_CONFIG_NOT_STARTED && systemState <= STATE_WIFI_CONFIG)
        return (true);
    return (false);
}

bool inNtpMode()
{
    if (systemState >= STATE_NTPSERVER_NOT_STARTED && systemState <= STATE_NTPSERVER_SYNC)
        return (true);
    return (false);
}

void addSetupButton(std::vector<setupButton> *buttons, const char *name, SystemState newState)
{
    setupButton button;
    button.name = name;
    button.newState = newState;
    buttons->push_back(button);
}

// Construct menu 'buttons' to allow user to pause on one and double tap to select it
void constructSetupDisplay(std::vector<setupButton> *buttons)
{
    buttons->clear();

    // It looks like we don't need "Mark"? TODO: check this!
    addSetupButton(buttons, "Base", STATE_BASE_NOT_STARTED);
    addSetupButton(buttons, "Rover", STATE_ROVER_NOT_STARTED);
    if (present.ethernet_ws5500 == true)
    {
        addSetupButton(buttons, "NTP", STATE_NTPSERVER_NOT_STARTED);
        addSetupButton(buttons, "Cfg Eth", STATE_CONFIG_VIA_ETH_NOT_STARTED);
        addSetupButton(buttons, "Cfg WiFi", STATE_WIFI_CONFIG_NOT_STARTED);
    }
    else
    {
        addSetupButton(buttons, "Config", STATE_WIFI_CONFIG_NOT_STARTED);
    }
    if (settings.enablePointPerfectCorrections)
    {
        addSetupButton(buttons, "Get Keys", STATE_KEYS_REQUESTED);
    }
    addSetupButton(buttons, "E-Pair", STATE_ESPNOW_PAIRING_NOT_STARTED);
    // If only one active profile do not show any profiles
    if (getProfileCount() > 1)
    {
        for (int x = 0; x < getProfileCount(); x++)
        {
            int profile = getProfileNumberFromUnit(x);
            if (profile >= 0)
            {
                setupButton button;
                button.name = &profileNames[profile][0];
                button.newState = STATE_PROFILE;
                button.newProfile = x; // paintProfile needs the unit
                buttons->push_back(button);
            }
        }
    }
    addSetupButton(buttons, "Exit", STATE_NOT_SET);
}
