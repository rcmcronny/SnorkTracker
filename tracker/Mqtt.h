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
  * @file Mqtt.h
  *
  * Communication with an MQTT server.
  */


#include <PubSubClient.h>

#define topic_deep_sleep             "/DeepSleep"              //!< Deep sleep on/off

#define topic_voltage                "/Voltage"                //!< Power supply voltage
#define topic_mAh                    "/mAh"                    //!< Power consumption
#define topic_mAhLowPower            "/mAhLowPower"            //!< Power consumption in low power
#define topic_alive                  "/Alive"                  //!< Alive time in sec

#define topic_power_on               "/PowerOn"                //!< switch power on/off
#define topic_gps_enabled            "/GpsEnabled"             //!< switch gps on/off
#define topic_send_on_move_every     "/SendOnMoveEverySec"     //!< mqtt send interval on moving
#define topic_send_on_non_move_every "/SendOnNonMoveEverySec"  //!< mqtt sending interval on non moving
#define topic_send_every             "/SendEverySec"           //!< mqtt sending interval

#define topic_temperature            "/BME280/Temperature"     //!< Temperature
#define topic_humidity               "/BME280/Humidity"        //!< Humidity
#define topic_pressure               "/BME280/Pressure"        //!< Pressure

#define topic_signal_quality         "/Gsm/SignalQuality"      //!< Signal quality
#define topic_batt_level             "/Gsm/BattLevel"          //!< Battery level of the gsm modul
#define topic_batt_volt              "/Gsm/BattVolt"           //!< Battery volt of the gsm modul

#define topic_lon                    "/Gps/Longitude"          //!< Gps longitude
#define topic_lat                    "/Gps/Latitude"           //!< Gps latitude
#define topic_alt                    "/Gps/Altitude"           //!< Gps altitude
#define topic_kmph                   "/Gps/Kmh"                //!< Gps moving speed

/**
  * MQTT client for sending the collected data to a MQTT server
  */
class MyMqtt : protected PubSubClient
{
protected:
   static MyOptions *g_myOptions;   //!< Static option pointer for the callback function.

public:
   static void mqttCallback(char* topic, byte* payload, unsigned int len);
   
protected:
   MyOptions &myOptions;            //!< Reference to the options. 
   MyData    &myData;               //!< Reference to the data.
   bool       publishInProgress;    //!< Are we publishing right now.

protected:
   bool mySubscribe(String subTopic);
   bool myPublish(String subTopic, String value);

public:
   MyMqtt(Client &client, MyOptions &options, MyData &data);
   ~MyMqtt();
   
   bool begin();
   void handleClient();
   
   bool waitingForMqtt();
};

/* ******************************************** */

/** Constructor/Destructor */
MyMqtt::MyMqtt(Client &client, MyOptions &options, MyData &data)
   : PubSubClient(client)
   , myOptions(options)
   , myData(data)
   , publishInProgress(false)
{
   g_myOptions = &options;
}
MyMqtt::~MyMqtt()
{
   g_myOptions = NULL;
}

/** Helper function to subscrbe on mqtt 
 *  It put the mqttName and id from options before the topic.
*/
bool MyMqtt::mySubscribe(String subTopic)
{
   String topic;

   topic = myOptions.mqttName + F("/") + myOptions.mqttId + subTopic;
   MyDbg((String) F("MyMqtt::subscribe: [") + topic + F("]"), true);
   return PubSubClient::subscribe(topic.c_str());
}

/** Helper function to publish on mqtt 
 *  It put the mqttName from optione before the topic.
*/
bool MyMqtt::myPublish(String subTopic, String value)
{
   bool ret = false;

   if (value.length() > 0) {
      String topic;

      topic = myOptions.mqttName + F("/") + myOptions.mqttId + subTopic;
      MyDbg((String) F("MyMqtt::publish: [") + topic + F("]=[") + value + F("]"), true);
      ret = PubSubClient::publish(topic.c_str(), value.c_str(), true);
   }
   return ret;
}

/** Check if we have to wait for sending mqtt data. */
bool MyMqtt::waitingForMqtt()
{
   if (publishInProgress) {
      return true;
   }
   if (myData.isMoving) {
      return secondsElapsed(myData.rtcData.lastMqttSendSec, myOptions.mqttSendOnMoveEverySec);
   } else {
      return secondsElapsed(myData.rtcData.lastMqttSendSec, myOptions.mqttSendOnNonMoveEverySec);
   }
   return false;
}

/** Sets the MQTT server settings */
bool MyMqtt::begin()
{
   MyDbg(F("MQTT:begin"), true);
   PubSubClient::setServer(myOptions.mqttServer.c_str(), myOptions.mqttPort);
   PubSubClient::setCallback(mqttCallback);
   return true;
}

/** Connect To the MQTT server and send the data when the time is right. */
void MyMqtt::handleClient()
{
   bool send = false;

   if (myData.isMoving) {
      send = secondsElapsed(myData.rtcData.lastMqttSendSec, myOptions.mqttSendOnMoveEverySec);
   } else {
      send = secondsElapsed(myData.rtcData.lastMqttSendSec, myOptions.mqttSendOnNonMoveEverySec);
   }
   if ((send || myData.mqttInitSend) && !publishInProgress) {
      publishInProgress = true;
      if (!PubSubClient::connected()) {
         for (int i = 0; !PubSubClient::connected() && i < 5; i++) {  
            MyDbg(F("Attempting MQTT connection..."), true);
            if (PubSubClient::connect(myOptions.mqttName.c_str(), myOptions.mqttUser.c_str(), myOptions.mqttPassword.c_str())) {  
               mySubscribe(topic_deep_sleep);
               mySubscribe(topic_power_on);
               mySubscribe(topic_gps_enabled);
               mySubscribe(topic_send_on_move_every);
               mySubscribe(topic_send_on_non_move_every);
               MyDbg(F(" connected"), true);
            } else {  
               MyDbg((String) F("   Mqtt failed, rc = ") + String(PubSubClient::state()), true);
               MyDbg(F(" Try again in 5 seconds"), true);
               MyDelay(5000);
               MyDbg(F("."), true, false);
            }  
         }  
      }
      if (PubSubClient::connected()) {
         MyDbg(F("Attempting MQTT publishing"), true);

         /*
         if (myData.mqttInitSend) { // On power on or SaveSettings we publish the subscribed values.
            myPublish(topic_deep_sleep,             String(g_myOptions->isDeepSleepEnabled));
            myPublish(topic_power_on,               String(g_myOptions->powerOn));
#ifdef SIM808_CONNECTED
            myPublish(topic_gps_enabled,            String(g_myOptions->isGpsEnabled));
            myPublish(topic_send_on_move_every,     String(g_myOptions->mqttSendOnMoveEverySec));
            myPublish(topic_send_on_non_move_every, String(g_myOptions->mqttSendOnNonMoveEverySec));
#else 
            myPublish(topic_send_every,             String(g_myOptions->mqttSendOnNonMoveEverySec));
#endif
            myData.mqttInitSend = false;
         }
         */

         myPublish(topic_voltage,     String(myData.voltage, 2));
         myPublish(topic_mAh,         String(myData.getPowerConsumption()));
         myPublish(topic_mAhLowPower, String(myData.getLowPowerPowerConsumption()));
         myPublish(topic_alive,       formatInterval(myData.getActiveTimeSec()));

         myPublish(topic_temperature, String(myData.temperature));
         myPublish(topic_humidity,    String(myData.humidity));
         myPublish(topic_pressure,    String(myData.pressure));

#ifdef SIM808_CONNECTED
         myPublish(topic_signal_quality, myData.signalQuality);
         myPublish(topic_batt_level,     myData.batteryLevel);
         myPublish(topic_batt_volt,      myData.batteryVolt);

         if (myData.rtcData.lastGps.fixStatus) {
            myPublish(topic_lon,  myData.rtcData.lastGps.longitudeString());
            myPublish(topic_lat,  myData.rtcData.lastGps.latitudeString());
            myPublish(topic_alt,  myData.rtcData.lastGps.altitudeString());
            myPublish(topic_kmph, myData.rtcData.lastGps.kmphString());
         }
#endif
         myData.rtcData.mqttSendCount++;
         myData.rtcData.mqttLastSentTime = myData.rtcData.lastGps.time;
         myData.rtcData.lastMqttSendSec = secondsSincePowerOn();
         MyDbg(F("mqtt published"), true);
         MyDelay(5000);
      }
      publishInProgress = false;
   }
}

MyOptions *MyMqtt::g_myOptions = NULL;

/** Static function for MQTT callback on registered topics. */
void MyMqtt::mqttCallback(char* topic, byte* payload, unsigned int len) 
{
   if (topic == NULL || payload == NULL || len <= 0 || len > 200) {
      return;
   }

   String strTopic = String((char*)topic);

   payload[len] = '\0';
   MyDbg((String) F("Message arrived [") + strTopic + F("]:[ "), true);
   if (len) MyDbg((char *) payload, true);
   MyDbg(F("]"), true);

   if (MyMqtt::g_myOptions) {
      if (strTopic == g_myOptions->mqttName + topic_deep_sleep) {
         g_myOptions->isDeepSleepEnabled = atoi((char *) payload);
         MyDbg(strTopic + g_myOptions->isDeepSleepEnabled ? F(" - On") : F(" - Off"), true);
      }
      if (strTopic == g_myOptions->mqttName + topic_power_on) {
         g_myOptions->powerOn = atoi((char *) payload);
         MyDbg(strTopic + g_myOptions->powerOn ? F(" - On") : F(" - Off"), true);
      }
      if (strTopic == g_myOptions->mqttName + topic_gps_enabled) {
         g_myOptions->isGpsEnabled = atoi((char *) payload);
         MyDbg(strTopic + g_myOptions->isGpsEnabled ? F(" - Enabled") : F(" - Disabled"), true);
      }
      if (strTopic == g_myOptions->mqttName + topic_send_on_move_every) {
         g_myOptions->mqttSendOnMoveEverySec = atoi((char *) payload);
         MyDbg(strTopic + " - " + String(g_myOptions->mqttSendOnMoveEverySec), true);
      }
      if (strTopic == g_myOptions->mqttName + topic_send_on_non_move_every) {
         g_myOptions->mqttSendOnNonMoveEverySec = atoi((char *) payload);
         MyDbg(strTopic + " - " + String(g_myOptions->mqttSendOnNonMoveEverySec), true);
      }
      if (strTopic == g_myOptions->mqttName + topic_send_every) {
         g_myOptions->mqttSendOnNonMoveEverySec = atoi((char *)payload);
         MyDbg(strTopic + " - " + String(g_myOptions->mqttSendOnNonMoveEverySec), true);
      }
   }
}
