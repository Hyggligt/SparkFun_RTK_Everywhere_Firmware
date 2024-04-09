#ifdef COMPILE_POINTPERFECT_LIBRARY

// Feed the PointPerfect Library with NMEA+RTCM
void updatePplTask(void *e)
{
    // Start notification
    online.updatePplTaskRunning = true;
    if (settings.printTaskStartStop)
        systemPrintln("updatePplTask started");

    // Verify that the task is still running
    while (online.updatePplTaskRunning)
    {
        // Display an alive message
        if (PERIODIC_DISPLAY(PD_TASK_UPDATE_PPL))
        {
            PERIODIC_CLEAR(PD_TASK_UPDATE_PPL);
            systemPrintln("UpdatePplTask running");
        }

        if (pplNewRtcmNmea || pplNewSpartn) // Decide when to call PPL_GetRTCMOutput
        {
            if (pplNewRtcmNmea)
                pplNewRtcmNmea = false;
            if (pplNewSpartn)
                pplNewSpartn = false;

            uint32_t rtcmLength;

            // Check if the PPL has generated any RTCM. If it has, push it to the GNSS.
            ePPL_ReturnStatus result = PPL_GetRTCMOutput(pplRtcmBuffer, PPL_MAX_RTCM_BUFFER, &rtcmLength);
            if (result == ePPL_Success)
            {
                if (rtcmLength > 0)
                {
                    updateCorrectionsLastSeen(CORR_LBAND);
                    if (isHighestRegisteredCorrectionsSource(CORR_LBAND))
                    {
                        updateZEDCorrectionsSource(1); // Set ZED SOURCE to 1 (L-Band) if needed
                        
                        gnssPushRawData(pplRtcmBuffer, rtcmLength);

                        if (settings.debugCorrections == true && !inMainMenu)
                            systemPrintf("Received %d RTCM bytes from PPL. Pushing to the GNSS.\r\n", rtcmLength);
                    }
                    else
                    {
                        if (settings.debugCorrections == true && !inMainMenu)
                            systemPrintf("Received %d RTCM bytes from PPL. NOT pushed to the GNSS due to priority.\r\n", rtcmLength);
                    }
                }
            }
            else
            {
                if (settings.debugCorrections == true && !inMainMenu)
                    systemPrintf("PPL_GetRTCMOutput Result: %s\r\n", PPLReturnStatusToStr(result));
            }
        }

        // Check to see if our key has expired
        if (millis() > pplKeyExpirationMs)
        {
            if (settings.debugCorrections == true)
                systemPrintln("Key has expired. Going to new key.");

            // Get a key from NVM
            char pointPerfectKey[33]; // 32 hexadecimal digits = 128 bits = 16 Bytes
            if (getUsablePplKey(pointPerfectKey, sizeof(pointPerfectKey)) == false)
            {
                if (settings.debugCorrections == true)
                    systemPrintln("Unable to get usable key");

                online.ppl = false; // Take PPL offline
            }
            else
            {
                // Key obtained from NVM
                ePPL_ReturnStatus result = PPL_SendDynamicKey(pointPerfectKey, strlen(pointPerfectKey));
                if (settings.debugCorrections == true)
                    systemPrintf("PPL_SendDynamicKey: %s\r\n", PPLReturnStatusToStr(result));

                if (result != ePPL_Success)
                {
                    systemPrintln("PointPerfect Library Key Invalid");
                    online.ppl = false; // Take PPL offline
                }
            }

            online.updatePplTaskRunning = false; // Stop task either because new key failed or we need to apply new key
        }

        feedWdt();
        taskYIELD();
    }

    // Stop notification
    if (settings.printTaskStartStop)
        systemPrintln("Task updatePplTask stopped");
    online.updatePplTaskRunning = false;
    vTaskDelete(NULL);
}

// Begin the PointPerfect Library and give it the current key
void beginPPL()
{
    if (present.needsExternalPpl == false)
        return;

    if (online.psram == false)
    {
        systemPrintln("PointPerfect Library requires PSRAM to run");
        return;
    }

    // Get a key from NVM
    char pointPerfectKey[33]; // 32 hexadecimal digits = 128 bits = 16 Bytes
    if (getUsablePplKey(pointPerfectKey, sizeof(pointPerfectKey)) == false)
    {
        if (settings.debugCorrections == true)
            systemPrintln("Unable to get usable key");
        return;
    }

    reportHeapNow(false);

    // PPL_MAX_RTCM_BUFFER is 3345 bytes so we create it on the heap
    if (online.psram == true)
        pplRtcmBuffer = (uint8_t *)ps_malloc(PPL_MAX_RTCM_BUFFER);
    else
        pplRtcmBuffer = (uint8_t *)malloc(PPL_MAX_RTCM_BUFFER);

    if (!pplRtcmBuffer)
    {
        systemPrintln("ERROR: Failed to allocate rtcmBuffer");
        return;
    }

    bool successfulInit = true;

    ePPL_ReturnStatus result = PPL_Initialize(PPL_CFG_DEFAULT_CFG); // IP and L-Band support

    if (result != ePPL_Success)
    {
        systemPrintln("PointPerfect Library Failed to Initialize");
        successfulInit &= false;
    }

    if (settings.debugCorrections == true)
        systemPrintf("PPL_Initialize: %s\r\n", PPLReturnStatusToStr(result));

    result = PPL_SendDynamicKey(pointPerfectKey, strlen(pointPerfectKey));
    if (settings.debugCorrections == true)
        systemPrintf("PPL_SendDynamicKey: %s\r\n", PPLReturnStatusToStr(result));

    if (result != ePPL_Success)
    {
        systemPrintln("PointPerfect Library Key Invalid");
        successfulInit &= false;
    }

    online.ppl = successfulInit;

    if (online.ppl)
    {
        systemPrintf("PointPerfect Library Online: %s\r\n", PPL_SDK_VERSION);

        TaskHandle_t taskHandle;

        // Starts task for feeding NMEA+RTCM to PPL
        if (online.updatePplTaskRunning == false)
            xTaskCreate(updatePplTask,
                        "UpdatePpl",            // Just for humans
                        updatePplTaskStackSize, // Stack Size
                        nullptr,                // Task input parameter
                        updatePplTaskPriority,
                        &taskHandle); // Task handle
    }
    else
        systemPrintln("PointPerfect Library failed to start");

    reportHeapNow(false);
}

// Start the PPL if needed
// Because the key for the PPL expires every ~28 days, we use updatePPL to first apply keys, and
// restart the PPL when new keys need to be applied
void updatePPL()
{
    if (online.ppl == false && (settings.enablePointPerfectCorrections))
    {
        // Start PPL only after GNSS is outputting appropriate NMEA+RTCM, we have a key, and the MQTT broker is
        // connected. Don't restart the PPL if we've already tried
        if (pplAttemptedStart == false)
        {
            if ((strlen(settings.pointPerfectCurrentKey) > 0) && (pplGnssOutput == true) &&
                (pplMqttCorrections == true))
            {
                pplAttemptedStart = true;

                beginPPL(); // Initialize PointPerfect Library
            }
        }
    }

    // The PPL is fed during updatePplTask()
}

// Checks the current and next keys for expiration
// Returns true if a key is available; key will be loaded into the given buffer
bool getUsablePplKey(char *keyBuffer, int keyBufferSize)
{
    if (strlen(settings.pointPerfectCurrentKey) == 0)
    {
        systemPrintln("PointPerfect Library: No keys available");
        return false;
    }

    if (online.rtc == false)
    {
        // If we don't have RTC we can't calculate days to expire
        // Assume current key
        strncpy(keyBuffer, settings.pointPerfectCurrentKey, keyBufferSize);

        if (settings.debugCorrections == true)
            systemPrintln("No RTC for key expiration calculation");
        return true;
    }

    int daysRemainingCurrent =
        daysFromEpoch(settings.pointPerfectCurrentKeyStart + settings.pointPerfectCurrentKeyDuration + 1);

    int daysRemainingNext = -1;
    if (strlen(settings.pointPerfectNextKey) > 0)
        daysRemainingNext = daysFromEpoch(settings.pointPerfectNextKeyStart + settings.pointPerfectNextKeyDuration + 1);

    if (settings.debugCorrections == true)
    {
        pointperfectPrintKeyInformation();
        systemPrintf("Days remaining until current key expires: %d\r\n", daysRemainingCurrent);
        systemPrintf("Days remaining until next key expires: %d\r\n", daysRemainingNext);
    }

    if (daysRemainingCurrent >= 0) // Use the current key
    {
        pplKeyExpirationMs =
            secondsFromEpoch(settings.pointPerfectCurrentKeyStart + settings.pointPerfectCurrentKeyDuration) * 1000;

        if (settings.debugCorrections == true)
            systemPrintf("Seconds remaining until key expires: %d\r\n", pplKeyExpirationMs / 1000);

        strncpy(keyBuffer, settings.pointPerfectCurrentKey, keyBufferSize);
        return (true);
    }
    else if (daysRemainingNext >= 0) // Use next key
    {
        pplKeyExpirationMs =
            secondsFromEpoch(settings.pointPerfectNextKeyStart + settings.pointPerfectNextKeyDuration) * 1000;

        if (settings.debugCorrections == true)
            systemPrintf("Seconds remaining until key expires: %d\r\n", pplKeyExpirationMs / 1000);

        strncpy(keyBuffer, settings.pointPerfectNextKey, keyBufferSize);
        return (true);
    }
    else
    {
        // Both keys are expired
        return (false);
    }

    return (false);
}

// Send GGA/ZDA/RTCM to the PPL
bool sendGnssToPpl(uint8_t *buffer, int numDataBytes)
{
    if (online.ppl == true)
    {
        ePPL_ReturnStatus result = PPL_SendRcvrData(buffer, numDataBytes);
        if (result != ePPL_Success)
        {
            if (settings.debugCorrections == true)
                systemPrintf("PPL_SendRcvrData Result: %s\r\n", PPLReturnStatusToStr(result));
            return false;
        }
        return true;
    }
    else
    {
        pplGnssOutput = true; // Notify updatePPL() that GNSS is outputting NMEA/RTCM
    }

    return false;
}

// Send Spartn packets from PointPerfect (either IP or L-Band) to PPL
bool sendSpartnToPpl(uint8_t *buffer, int numDataBytes)
{
    if (online.ppl == true)
    {

        ePPL_ReturnStatus result = PPL_SendSpartn(buffer, numDataBytes);
        if (result != ePPL_Success)
        {
            if (settings.debugCorrections == true)
                systemPrintf("ERROR processRXMPMP PPL_SendAuxSpartn: %s\r\n", PPLReturnStatusToStr(result));
            return false;
        }
        return true;
    }
    else
    {
        pplMqttCorrections = true; // Notify updatePPL() that MQTT is online
    }

    return false;
}

// Print human-readable PPL status
const char *PPLReturnStatusToStr(ePPL_ReturnStatus status)
{
    switch (status)
    {
    case ePPL_Success:
        return ("operation was successful");
    case ePPL_IncorrectLibUsage:
        return ("incorrect usage of the library");
    case ePPL_LibInitFailed:
        return ("library failed to initialize");
    case ePPL_LibExpired:
        return ("library has expired");
    case ePPL_LBandChannelNotEnabled:
        return ("L-Band channel has not been initialized");
    case ePPL_IPChannelNotEnabled:
        return ("IP channel has not been initialized");
    case ePPL_AuxChannelNotEnabled:
        return ("Aux channel has not been initialized");
    case ePPL_NoDynamicKey:
        return ("stream is encrypted and no valid dynamic key found");
    case ePPL_FailedDynKeyLibPush:
        return ("key push failed");
    case ePPL_InvalidDynKey:
        return ("invalid key format");
    case ePPL_IncorrectDynKey:
        return ("key does not match the key used for data encryption");
    case ePPL_RcvPosNotAvailable:
        return ("GGA position not available");
    case ePPL_LeapSecsNotAvailable:
        return ("leap seconds not available");
    case ePPL_AreaDefNotAvailableForPos:
        return ("position is outside of Point Perfect coverage");
    case ePPL_TimeNotResolved:
        return ("time is not resolved");
    case ePPL_PotentialBuffOverflow:
        return ("potential buffer overflow");
    case ePPL_UnknownState:
    default:
        return ("unknown state");
    }
}

void pointperfectPrintKeyInformation()
{
    systemPrintf("  pointPerfectCurrentKey: %s\r\n", settings.pointPerfectCurrentKey);
    systemPrintf(
        "  pointPerfectCurrentKeyStart: %lld - %s\r\n", settings.pointPerfectCurrentKeyStart,
        printDateFromUnixEpoch(settings.pointPerfectCurrentKeyStart / 1000)); // printDateFromUnixEpoch expects seconds
    systemPrintf("  pointPerfectCurrentKeyDuration: %lld - %s\r\n", settings.pointPerfectCurrentKeyDuration,
                 printDaysFromDuration(settings.pointPerfectCurrentKeyDuration));
    systemPrintf("  pointPerfectNextKey: %s\r\n", settings.pointPerfectNextKey);
    systemPrintf("  pointPerfectNextKeyStart: %lld - %s\r\n", settings.pointPerfectNextKeyStart,
                 printDateFromUnixEpoch(settings.pointPerfectNextKeyStart / 1000));
    systemPrintf("  pointPerfectNextKeyDuration: %lld - %s\r\n", settings.pointPerfectNextKeyDuration,
                 printDaysFromDuration(settings.pointPerfectNextKeyDuration));
}

#endif // COMPILE_POINTPERFECT_LIBRARY
