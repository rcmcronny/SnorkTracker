/*
   Copyright (C) 2018 SFini

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
/**
  * @file Voltage.h
  *
  * Class to read the power supply voltage.
  */

#define ANALOG_FACTOR 0.03 //!< Factor to the analog voltage divider

/**
  * Voltage Reader. Works with the voltage divider resistors and the analog input reader.
  */
class MyVoltage
{
protected:
   MyOptions      &myOptions;        //!< Reference to global options
   MyData         &myData;           //!< Reference to global data

   uint32_t       lowPowerStartSec;  //!< Switch off timestamp

public:
   MyVoltage(MyOptions &options, MyData &data);

   bool begin();

   void readVoltage();
};

/* ******************************************** */

/** Constructor */
MyVoltage::MyVoltage(MyOptions &options, MyData &data)
   : myOptions(options)
   , myData(data)
   , lowPowerStartSec(0)
{
}

/** Reads the voltage at startup. */
bool MyVoltage::begin()
{
   MyDbg("MyVoltage::begin");
   myData.voltage    = ANALOG_FACTOR * analogRead(A0); // Volt
   myData.isLowPower = myData.voltage < myOptions.powerSaveModeVoltage;
   lowPowerStartSec  = millis() / 1000;
}

/** Reads the power supply voltage and save the value in the data class. 
  * Add the lowPower time to the lowPowerActive and lowPowerPowerOn time. 
  */
void MyVoltage::readVoltage()
{
   bool     isLowPower = false;
   uint32_t currSec    = millis() / 1000;
   
   myData.voltage = ANALOG_FACTOR * analogRead(A0); // Volt
   isLowPower     = myData.voltage < myOptions.powerSaveModeVoltage;

   if (myData.isLowPower && !isLowPower) { // Change to high power
      uint32_t lowPowerSec = currSec - lowPowerStartSec;

      myData.rtcData.lowPowerActiveTimeSec += lowPowerSec;
      if (myData.isPowerOn) {
         myData.rtcData.lowPowerPowerOnTimeSec += lowPowerSec;
      }
      MyDbg("Change to high power (V): " + String(myData.voltage, 1));
   }
   if (!myData.isLowPower && isLowPower) { // Change to low power
      lowPowerStartSec = currSec;
      MyDbg("Change to low power (V): " + String(myData.voltage, 1));
   }
   myData.isLowPower = isLowPower;
}