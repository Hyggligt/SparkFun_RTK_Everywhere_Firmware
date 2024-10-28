// User has pressed the power button to turn on the system
// Was it an accidental bump or do they really want to turn on?
// Let's make sure they continue to press for 500ms
void powerOnCheck()
{
    powerPressedStartTime = millis();
    if (pin_powerSenseAndControl != PIN_UNDEFINED)
        if (digitalRead(pin_powerSenseAndControl) == LOW)
            delay(500);

    if (FIRMWARE_VERSION_MAJOR == 99)
    {
        // Do not check button if this is a locally compiled developer firmware
    }
    else
    {
        if (pin_powerSenseAndControl != PIN_UNDEFINED)
            if (digitalRead(pin_powerSenseAndControl) != LOW)
                powerDown(false); // Power button tap. Returning to off state.
    }

    powerPressedStartTime = 0; // Reset var to return to normal 'on' state
}

// If we have a power button tap, or if the display is not yet started (no I2C!)
// then don't display a shutdown screen
void powerDown(bool displayInfo)
{
    // Disable SD card use
    endSD(false, false);

    gnss->standby(); // Put the GNSS into standby - if possible

    // Prevent other tasks from logging, even if access to the microSD card was denied
    online.logging = false;

    // If we are in configureViaEthernet mode, we need to shut down the async web server
    // otherwise it causes a core panic and badness at the restart
    if (configureViaEthernet)
        ethernetWebServerStopESP32W5500();

    if (displayInfo == true)
    {
        displayShutdown();
        delay(2000);
    }

    if (present.peripheralPowerControl == true)
        peripheralsOff();

    if (pin_powerSenseAndControl != PIN_UNDEFINED)
    {
        // Change the button input to an output
        // Note: on the original Facet, pin_powerSenseAndControl could be pulled low to
        //       turn the power off (slowly). No RTK-Everywhere devices use that circuit.
        //       Pulling it low on Facet mosaic does no harm.
        pinMode(pin_powerSenseAndControl, OUTPUT);
        digitalWrite(pin_powerSenseAndControl, LOW);
    }

    if (present.fastPowerOff == true)
    {
        pinMode(pin_powerFastOff, OUTPUT); // Ensure pin is an output
        digitalWrite(pin_powerFastOff, present.invertedFastPowerOff ? HIGH : LOW);
    }

    while (1)
    {
        // We should never get here but good to know if we do
        systemPrintln("Device powered down");
        delay(250);
    }
}
void buttonRead()
{
    // Check for direct button
    if (online.button == true)
        userBtn->read();
    
}

//Check if a previously pressed button has been released
bool buttonReleased()
{
    // Check for direct button
    if (online.button == true)
        return (userBtn->wasReleased());

    return (false);
}

//Check if a button has been pressed for a certain amount of time
bool buttonPressedFor(uint16_t maxTime)
{
    // Check for direct button
    if (online.button == true)
        return (userBtn->pressedFor(2100));

    return (false);
}