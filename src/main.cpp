#include <UIPEthernet.h>

// MAC address for your Ethernet shield (must be unique on your network)
byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};

// Ethernet client object
EthernetClient client;

// Relay pins (connect the IN pins of JQC3F-05VDC-C to these digital pins)
const int relay1Pin = 3; // Relay 1
const int relay2Pin = 4; // Relay 2

// State flags
bool prevPowerLowForRelay2 = false;
bool pendingRelay1Off = false;

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
  String currentKey = "";

  // Process response line by line
  while (client.connected() || client.available())
  {
    if (client.available())
    {
      char c = client.read();

      if (c == '\n' && bufferIndex > 0)
      {
        lineBuffer[bufferIndex] = '\0'; // Null-terminate the string

        String line = String(lineBuffer);
        line.trim(); // Remove leading/trailing whitespace

        // Check if we've reached the JSON data
        if (line.indexOf("{") != -1)
        {
          inJson = true;
        }

        if (inJson)
        {
          // Detect start of object for key
          int colonPos = line.indexOf(":");
          if (colonPos != -1 && line.endsWith("{"))
          {
            String keyPart = line.substring(0, colonPos);
            keyPart.trim();
            if (keyPart.startsWith("\"") && keyPart.endsWith("\""))
            {
              currentKey = keyPart.substring(1, keyPart.length() - 1);
              Serial.println(currentKey);
            }
          }
          else if (currentKey != "" && line.startsWith("\"value\" :"))
          {
            // Extract the value
            int valueStart = line.indexOf(":") + 1;
            int valueEnd = line.indexOf(",", valueStart);
            if (valueEnd == -1)
              valueEnd = line.length();
            String valueStr = line.substring(valueStart, valueEnd);
            valueStr.trim();
            float value = valueStr.toFloat();

            // Assign to the appropriate variable
            if (currentKey == "Power_P_Generate")
            {
              powerGenerate = value;
              valuesFound = true;
            }
            else if (currentKey == "Power_P_Load")
            {
              powerLoad = value;
              valuesFound = true;
            }
            else if (currentKey == "Power_P_Grid")
            {
              powerGrid = value;
              valuesFound = true;
            }

            currentKey = ""; // Reset after extracting value
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
    delay(5000); // Increased to 15 seconds
  }
  else
  {
    Serial.println("No values extracted");
  }
}