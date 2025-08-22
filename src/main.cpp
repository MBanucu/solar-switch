#include <UIPEthernet.h>
#include <ctype.h>
#include <string.h>

// MAC address for your Ethernet shield (must be unique on your network)
byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};

// Ethernet client object
EthernetClient client;

// Relay pins (connect the IN pins of JQC3F-05VDC-C to these digital pins)
const int relay1Pin = 3; // Relay 1
const int relay2Pin = 4; // Relay 2

const size_t keyLength = 35;

// State flags
bool prevPowerLowForRelay2 = false;
bool pendingRelay1Off = false;

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
  Serial.begin(9600); // Moved to the first command

  // Wait for serial port to connect (important for Nano)
  while (!Serial)
  {
    ; // Wait for serial port to be ready
  }
  // Send initial message to confirm USB communication
  Serial.println("Arduino Nano USB Test: Serial communication started!");

  // Initialize relay pins
  pinMode(relay1Pin, OUTPUT);
  pinMode(relay2Pin, OUTPUT);
  digitalWrite(relay1Pin, LOW); // Start with relay 1 off (active HIGH)
  digitalWrite(relay2Pin, LOW); // Start with relay 2 off (active HIGH)

  // Start Ethernet with DHCP
  if (Ethernet.begin(mac) == 0)
  {
    Serial.println("Failed to configure Ethernet using DHCP");
    while (1)
      ; // Halt if DHCP fails
  }
  Serial.print("IP address: ");
  Serial.println(Ethernet.localIP());

  delay(1000); // Allow network to stabilize
}

void loop()
{
  // Attempt to connect to http://pv-banucu/components/5/0/?print=names
  if (client.connect("pv-banucu", 80))
  {
    Serial.println("Connected to pv-banucu");

    // Send HTTP GET request
    client.println("GET /components/5/0/?print=names HTTP/1.1");
    client.println("Host: pv-banucu");
    client.println("Connection: close");
    client.println();
  }
  else
  {
    Serial.println("Connection failed");
    delay(1000); // Wait before retrying
    return;
  }

  bool inJson = false;
  char lineBuffer[56]; // Small buffer to hold each line
  uint8_t bufferIndex = 0;
  float powerGenerate = 0, powerLoad = 0, powerGrid = 0;
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
          else if (currentKey[0] != '\0' && strncmp(lineBuffer, "\"value\" :", 9) == 0)
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
              if (strcmp(currentKey, "Power_P_Generate") == 0)
              {
                powerGenerate = value;
                valuesFound = true;
              }
              else if (strcmp(currentKey, "Power_P_Load") == 0)
              {
                powerLoad = value;
                valuesFound = true;
              }
              else if (strcmp(currentKey, "Power_P_Grid") == 0)
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
  Serial.println("Disconnected");

  // Print extracted values if found
  if (valuesFound)
  {
    Serial.print("Power_P_Generate: ");
    Serial.println(powerGenerate);
    Serial.print("Power_P_Load: ");
    Serial.println(powerLoad);
    Serial.print("Power_P_Grid: ");
    Serial.println(powerGrid);

    // Check for delayed relay 1 off
    if (pendingRelay1Off && powerGrid > 0)
    {
      digitalWrite(relay1Pin, LOW);
      Serial.println("Delayed Relay 1 switched OFF");
    }
    pendingRelay1Off = false; // Reset after check

    // On logic for relays
    if (powerGrid < -1200)
    {
      digitalWrite(relay1Pin, HIGH);
      Serial.println("Relay 1 switched ON");
    }

    if (prevPowerLowForRelay2 && powerGrid < -600)
    {
      digitalWrite(relay2Pin, HIGH);
      Serial.println("Relay 2 switched ON");
    }

    // Off logic for relays
    if (powerGrid > 0)
    {
      if (digitalRead(relay2Pin) == HIGH)
      {
        digitalWrite(relay2Pin, LOW);
        Serial.println("Relay 2 switched OFF");
        pendingRelay1Off = true;
      }
      else
      {
        digitalWrite(relay1Pin, LOW);
        Serial.println("Relay 1 switched OFF");
      }
    }

    // Update flags for next cycle
    prevPowerLowForRelay2 = (powerGrid < -1200);
    delay(5000);
  }
  else
  {
    Serial.println("No values extracted");
  }
}