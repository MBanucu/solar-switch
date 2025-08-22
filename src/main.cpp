#include <UIPEthernet.h>

// MAC address for your Ethernet shield (must be unique on your network)
byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};

// Ethernet client object
EthernetClient client;

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
    delay(5000); // Wait before retrying
    return;
  }

  // Read and print the response
  while (client.connected() || client.available())
  {
    if (client.available())
    {
      String line = client.readStringUntil('\n');
      Serial.println(line);
    }
  }

  client.stop();
  Serial.println("Disconnected");

  delay(5000); // Wait 5 seconds before next attempt
}