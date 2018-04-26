/*
  This is the code I use for temperature monitoring in my house.

  I assert no copyright beyond CC0, and besides, there's so much
  cutnpaste in this that it might not be considered a work in its own
  right.

  Kjetil Kjernsmo <kjetil@kjernsmo.net>
*/

#include <OneWire.h>
#include <DallasTemperature.h>
#include <Ethernet.h>

// Data wire is plugged into port 2 on the Arduino
#define ONE_WIRE_BUS 2
// How many sensors are there on the bus?
#define NUMBER_OF_SENSORS 6
// What precision do we want?
#define TEMPERATURE_PRECISION 12




// assign a MAC address for the Ethernet controller.
byte mac[] = {
  0xDE, 0xAD, 0xBE, 0xEF, 0x01, 0x01
};
// assign an IP address for the controller:
IPAddress ip(172, 22, 1, 2);


// Initialize the Ethernet server library
// with the IP address and port you want to use
// (port 80 is default for HTTP):
EthernetServer server(80);

// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature. 
DallasTemperature sensors(&oneWire);

// arrays to hold device addresses
DeviceAddress Thermometers[NUMBER_OF_SENSORS];


// function to print a device address
void printAddress(DeviceAddress deviceAddress)
{
  for (uint8_t i = 0; i < 8; i++)
  {
    Serial.print("0x");
    // zero pad the address if necessary
    if (deviceAddress[i] < 16) Serial.print("0");
    Serial.print(deviceAddress[i], HEX);
    Serial.print(", ");
  }
}

// function to print the temperature for a device
void printTemperature(DeviceAddress deviceAddress)
{
  float tempC = sensors.getTempC(deviceAddress);
  Serial.print(F("Temp C: "));
  Serial.print(tempC);
}

// function to print a device's resolution
void printResolution(DeviceAddress deviceAddress)
{
  Serial.print(F("Resolution: "));
  Serial.print(sensors.getResolution(deviceAddress));
  Serial.println();    
}

// main function to print information about a device
void printData(DeviceAddress deviceAddress)
{
  Serial.print(F("Device Address: "));
  printAddress(deviceAddress);
  Serial.print(" ");
  printTemperature(deviceAddress);
  Serial.println();
}

void setup(void)
{
  // start serial port
  Serial.begin(9600);
  Serial.println(F("Dallas Temperature IC Web Server"));

  delay(1000);

  sensors.begin();

  Ethernet.begin(mac, ip);
  server.begin();


  // locate devices on the bus
  Serial.print(F("Locating devices..."));
  Serial.print(F("Found "));
  uint8_t countThermo = sensors.getDeviceCount();
  Serial.print(countThermo, DEC);
  Serial.println(F(" devices."));
  if (countThermo != NUMBER_OF_SENSORS) {
    Serial.print(F("But you declared "));
    Serial.print(NUMBER_OF_SENSORS);
    Serial.println(F(" devices with NUMBER_OF_SENSORS"));
  }


  // report parasite power requirements
  Serial.print(F("Parasite power is: ")); 
  if (sensors.isParasitePowerMode()) Serial.println("ON");
  else Serial.println("OFF");

  // Search for devices on the bus and assign based on an index. 
  for (uint8_t i = 0; i < countThermo; i++) {
    if (!sensors.getAddress(Thermometers[i], i)) {
      Serial.print(F("Unable to find address for Device ")); 
      Serial.println(i);
    }
  }

  // show the addresses we found on the bus
  for (uint8_t i = 0; i < countThermo; i++) {
    Serial.print(F("Address for device "));
    Serial.print(i);
    Serial.print(": ");
    printAddress(Thermometers[i]);
    Serial.println();
    sensors.setResolution(Thermometers[i], TEMPERATURE_PRECISION);

    Serial.print(F("Device "));
    Serial.print(i);
    Serial.print(F(" Resolution: "));
    Serial.print(sensors.getResolution(Thermometers[i]), DEC); 
    Serial.println();
  }
  Serial.println();
}


/*
 * Main function, calls the temperatures in a loop.
 */
void loop(void)
{ 
  EthernetClient client = server.available();
  if (client) {
    uint8_t countThermo = sensors.getDeviceCount();

    Serial.print(F("Requesting temperatures from "));
    Serial.print(countThermo);
    Serial.print(F(" devices..."));
    sensors.requestTemperatures();
    Serial.println("DONE");
    
    for (uint8_t i = 0; i < countThermo; i++) {
      printData(Thermometers[i]);
    }

    // print the device information
    Serial.println(F("Got a client"));
    // an http request ends with a blank line
    boolean currentLineIsBlank = true;
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        // if you've gotten to the end of the line (received a newline
        // character) and the line is blank, the http request has ended,
        // so you can send a reply
        if (c == '\n' && currentLineIsBlank) {
          if (countThermo < 1) {
            client.println(F("HTTP/1.1 500 Internal Server Error"));
            client.println(F("Content-Type: text/plain"));
            client.println();
            client.println(F("Found no sensors attached."));
          } else {
            // send a standard http response header
            client.println(F("HTTP/1.1 200 OK"));
            client.println(F("Content-Type: application/json"));
            client.println();
            
            client.println("{");
            for (uint8_t i = 0; i < countThermo; i++) {
              client.print("\"");
              for (uint8_t j = 0; j < 8; j++) {
                // zero pad the address if necessary
                if (Thermometers[i][j] < 16) client.print("0");
                client.print(Thermometers[i][j], HEX);
              }
              client.print("\":");
              client.print(sensors.getTempC(Thermometers[i]));
              if (i < countThermo-1) {
                client.println(",");
              } else {
                client.println();
              }
            }
            client.println("}");
          }
          break;
        }
        if (c == '\n') {
          // you're starting a new line
          currentLineIsBlank = true;
        } else if (c != '\r') {
          // you've gotten a character on the current line
          currentLineIsBlank = false;
        }
      }
    }
    // give the web browser time to receive the data
    delay(1);
    // close the connection:
    client.stop();
  }
}
