#include <UIPEthernet.h>
#include <ctype.h>
#include <string.h>

// #define DEBUG

// MAC address for your Ethernet shield (must be unique on your network)
const byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};

// Ethernet client object
EthernetClient client;

// Relay pins (connect the IN pins of JQC3F-05VDC-C to these digital pins)
const int relayPinScooter = 3; // Relay 1
const int relayPinCar = 4;     // Relay 2

const size_t keyLength = 35;
const float powerScooter = 600;
const float powerCar = 1200;

void trim(char *str)
{
  char *start = str;
  while (*start && isspace((unsigned char)*start))
    start++;
  if (start != str)
  {
    memmove(str, start, strlen(start) + 1);
  }
  char *end = str + strlen(str) - 1;
  while (end >= str && isspace((unsigned char)*end))
  {
    *end-- = '\0';
  }
}

void setup()
{
#ifdef DEBUG
  Serial.begin(9600); // Moved to the first command

  // Wait for serial port to connect (important for Nano)
  while (!Serial)
  {
    ; // Wait for serial port to be ready
  }
  // Send initial message to confirm USB communication
  Serial.println("Arduino Nano USB Test: Serial communication started!");
#endif

  // Initialize relay pins
  pinMode(relayPinScooter, OUTPUT);
  pinMode(relayPinCar, OUTPUT);
  digitalWrite(relayPinScooter, LOW); // Start with relay 1 off (active HIGH)
  digitalWrite(relayPinCar, LOW);     // Start with relay 2 off (active HIGH)

  // Start Ethernet with DHCP
  if (Ethernet.begin(mac) == 0)
  {
#ifdef DEBUG
    Serial.println("Failed to configure Ethernet using DHCP");
#endif
    while (1)
      ; // Halt if DHCP fails
  }
#ifdef DEBUG
  Serial.print("IP address: ");
  Serial.println(Ethernet.localIP());
#endif

  delay(1000); // Allow network to stabilize
}

void loop()
{
  // Attempt to connect to http://pv-banucu/components/5/0/?print=names
  if (client.connect("pv-banucu", 80))
  {
#ifdef DEBUG
    Serial.println("Connected to pv-banucu");
#endif

    // Send HTTP GET request
    client.println("GET /components/5/0/?print=names HTTP/1.1");
    client.println("Host: pv-banucu");
    client.println("Connection: close");
    client.println();
  }
  else
  {
#ifdef DEBUG
    Serial.println("Connection failed");
#endif
    delay(1000); // Wait before retrying
    return;
  }

  bool inJson = false;
  char lineBuffer[56]; // Small buffer to hold each line
  uint8_t bufferIndex = 0;
  float powerGrid = 0;
  bool valuesFound = false;
  char currentKey[keyLength];
  currentKey[0] = '\0';

  // Process response line by line
  while (client.connected() || client.available())
  {
    if (client.available())
    {
      char c = client.read();

      if (c == '\n' && bufferIndex > 0)
      {
        lineBuffer[bufferIndex] = '\0'; // Null-terminate the string

        trim(lineBuffer); // Remove leading/trailing whitespace

        // Check if we've reached the JSON data
        if (strchr(lineBuffer, '{') != NULL)
        {
          inJson = true;
        }

        if (inJson)
        {
          // Detect start of object for key
          char *colon = strchr(lineBuffer, ':');
          size_t len = strlen(lineBuffer);
          if (colon != NULL && len > 0 && lineBuffer[len - 1] == '{')
          {
            char keyPart[keyLength];
            size_t keyLen = colon - lineBuffer;
            if (keyLen >= sizeof(keyPart))
              keyLen = sizeof(keyPart) - 1;
            strncpy(keyPart, lineBuffer, keyLen);
            keyPart[keyLen] = '\0';
            trim(keyPart);

            size_t keyPartLen = strlen(keyPart);
            if (keyPartLen >= 2 && keyPart[0] == '"' && keyPart[keyPartLen - 1] == '"')
            {
              strncpy(currentKey, keyPart + 1, keyPartLen - 2);
              currentKey[keyPartLen - 2] = '\0';
            }
          }
          else if (currentKey[0] != '\0' && strncmp(lineBuffer, "\"value\"", 7) == 0)
          {
            // Extract the value
            char *valueStartPtr = strchr(lineBuffer, ':') + 1;
            if (valueStartPtr != NULL)
            {
              char *valueEnd = strchr(valueStartPtr, ',');
              if (valueEnd == NULL)
                valueEnd = lineBuffer + strlen(lineBuffer);

              char valueStr[20];
              size_t valueLen = valueEnd - valueStartPtr;
              if (valueLen >= sizeof(valueStr))
                valueLen = sizeof(valueStr) - 1;
              strncpy(valueStr, valueStartPtr, valueLen);
              valueStr[valueLen] = '\0';
              trim(valueStr);

              float value = atof(valueStr);

              // Assign to the appropriate variable
              if (strcmp(currentKey, "Power_P_Grid") == 0)
              {
                powerGrid = value;
                valuesFound = true;
              }

              currentKey[0] = '\0'; // Reset after extracting value
            }
          }
        }

        bufferIndex = 0; // Reset buffer for next line
      }
      else if (c != '\r' && bufferIndex < sizeof(lineBuffer) - 1)
      {
        lineBuffer[bufferIndex++] = c; // Add character to buffer
      }
    }
  }

  client.stop();
#ifdef DEBUG
  Serial.println("Disconnected");
#endif

  // Print extracted values if found
  if (valuesFound)
  {
#ifdef DEBUG
    Serial.print("Power_P_Grid: ");
    Serial.println(powerGrid);
#endif

    // On logic for relays
    if (digitalRead(relayPinScooter) == LOW)
    {
      if (digitalRead(relayPinCar) == LOW)
      {
        if (powerGrid < -powerCar)
        {
          digitalWrite(relayPinCar, HIGH);
#ifdef DEBUG
          Serial.println("Relay car switched ON");
#endif
        }
        else if (powerGrid < -powerScooter)
        {
          digitalWrite(relayPinScooter, HIGH);
#ifdef DEBUG
          Serial.println("Relay scooter switched ON");
#endif
        }
      }
      else
      {
        if (powerGrid < -powerScooter)
        {
          digitalWrite(relayPinScooter, HIGH);
#ifdef DEBUG
          Serial.println("Relay scooter switched ON");
#endif
        }
      }
    }
    else
    {
      if (digitalRead(relayPinCar) == LOW)
      {
        if (powerGrid < -powerCar)
        {
          digitalWrite(relayPinCar, HIGH);
#ifdef DEBUG
          Serial.println("Relay car switched ON");
#endif
        }
      }
    }

    // Off logic for relays
    if (powerGrid > 0)
    {
      if (digitalRead(relayPinScooter) == HIGH)
      {
        digitalWrite(relayPinScooter, LOW);
#ifdef DEBUG
        Serial.println("Relay scooter switched OFF");
#endif
      }
      else if (digitalRead(relayPinCar) == HIGH)
      {
        digitalWrite(relayPinCar, LOW);
#ifdef DEBUG
        Serial.println("Relay car switched OFF");
#endif
      }
    }

    delay(15000);
  }
  else
  {
#ifdef DEBUG
    Serial.println("No values extracted");
#endif
  }
}