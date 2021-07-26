#include <SPI.h>
#include <SD.h>
#include <Wire.h>
#include <SparkFun_MAX1704x_Fuel_Gauge_Arduino_Library.h>

#define PIN_TX 17
#define PIN_RX 16
#define PIN_G0 15
#define PIN_G1 25
#define PIN_G2 26
#define PIN_G5 32
#define PIN_G6 33
#define PIN_SCL 22
#define PIN_SDA 21
#define PIN_CIPO 19
#define PIN_COPI 23
#define PIN_SCK 18
#define PIN_D1 27
#define PIN_D0 14
#define PIN_PWM1 12
#define PIN_PWM0 13
#define PIN_A1 35
#define PIN_A0 34

SFE_MAX1704X lipo(MAX1704X_MAX17048);

enum PACKET_HEADERS : uint8_t
{
  HDR_ALIVE = 0xa0,
  HDR_INPUT_PULLUP,
  HDR_INPUT_PULLDOWN,
  HDR_READ_ANALOG,
  HDR_READ_SD,
  HDR_SD_ERR,
  HDR_SD_DATA,
  HDR_ACK,
  HDR_LIPO,
  HDR_FUEL_GAUGE_ERR,
  HDR_GREETING,
};

enum CMD_DRIVE : uint8_t
{
  CMD_FLOAT,
  CMD_DRIVE_LOW,
  CMD_DRIVE_HIGH,
  CMD_INPUT_PULLUP,
  CMD_INPUT_PULLDOWN,
};

enum CMD_PINS : uint8_t
{
  CMD_PIN_G0,
  CMD_PIN_G1,
  CMD_PIN_G2,
  CMD_PIN_G5,
  CMD_PIN_G6,
  CMD_PIN_CIPO,
  CMD_PIN_COPI,
  CMD_PIN_SCK,
  CMD_PIN_D1,
  CMD_PIN_D0,
  CMD_PIN_PWM1,
  CMD_PIN_PWM0,
  CMD_PIN_A1,
  CMD_PIN_A0,
};

uint8_t buffer[3] = {0};
File theFile;

// This function gets analog values and sends them back.
// It also shifts the values to the right 2 places since we don't need precision - just a 
// go-no go test in here. This way the values read back can be sent back to FJ2 using
// a single byte per channel.
void ReadAnalog()
{
  uint16_t ch0 = analogRead(PIN_A0);
  uint16_t ch1 = analogRead(PIN_A1);
  Serial1.write(HDR_READ_ANALOG);
  Serial1.write(ch0 >> 2);
  Serial1.write(ch1 >> 2);
}

// Sets all pins as pullups
void SetPullup()
{
  pinMode(PIN_G0, INPUT_PULLUP);
  pinMode(PIN_G1, INPUT_PULLUP);
  pinMode(PIN_G2, INPUT_PULLUP);
  pinMode(PIN_G5, INPUT_PULLUP);
  pinMode(PIN_G6, INPUT_PULLUP);
  pinMode(PIN_SCL, INPUT_PULLUP);
  pinMode(PIN_SDA, INPUT_PULLUP);
  pinMode(PIN_CIPO, INPUT_PULLUP);
  pinMode(PIN_COPI, INPUT_PULLUP);
  pinMode(PIN_SCK, INPUT_PULLUP);
  pinMode(PIN_D1, INPUT_PULLUP);
  pinMode(PIN_D0, INPUT_PULLUP);
  pinMode(PIN_PWM1, INPUT_PULLUP);
  pinMode(PIN_PWM0, INPUT_PULLUP);

  // Acks FJ2
  Serial1.write(HDR_INPUT_PULLUP);
  Serial1.write(0x0);
  Serial1.write(0x0);
}

// Sets all pins as pulldowns
void SetPullDown()
{
  pinMode(PIN_G0, INPUT_PULLDOWN);
  pinMode(PIN_G1, INPUT_PULLDOWN);
  pinMode(PIN_G2, INPUT_PULLDOWN);
  pinMode(PIN_G5, INPUT_PULLDOWN);
  pinMode(PIN_G6, INPUT_PULLDOWN);
  pinMode(PIN_SCL, INPUT_PULLDOWN);
  pinMode(PIN_SDA, INPUT_PULLDOWN);
  pinMode(PIN_CIPO, INPUT_PULLDOWN);
  pinMode(PIN_COPI, INPUT_PULLDOWN);
  pinMode(PIN_SCK, INPUT_PULLDOWN);
  pinMode(PIN_D1, INPUT_PULLDOWN);
  pinMode(PIN_D0, INPUT_PULLDOWN);
  pinMode(PIN_PWM1, INPUT_PULLDOWN);
  pinMode(PIN_PWM0, INPUT_PULLDOWN);

  // Acks FJ2
  Serial1.write(HDR_INPUT_PULLDOWN);
  Serial1.write(0x0);
  Serial1.write(0x0);
}

// This function reads the SD card and send the text parsed in the test.txt file
// or an error header if the file could not be read.
void ReadSDCard()
{
  pinMode(23, INPUT_PULLUP);
  pinMode(5, INPUT_PULLUP);

  SPI.begin(18, 19, 23, 5);
  SD.begin(5);
  theFile = SD.open("/test.txt");

  if(theFile)
  {
    Serial1.write(HDR_SD_DATA);
    int data;
    while (theFile.available())
    {
      data = theFile.read();
      Serial1.write(data);
    }
    theFile.close();
  }
  else
  { 
    Serial1.write(HDR_SD_ERR);
    Serial1.write(0x0);
    Serial1.write(0x0);
  }
}

void setup()
{
  // Set all pins high impedance
  pinMode(PIN_TX, INPUT);
  pinMode(PIN_RX, INPUT);
  pinMode(PIN_G0, INPUT);
  pinMode(PIN_G1, INPUT);
  pinMode(PIN_G2, INPUT);
  pinMode(PIN_G5, INPUT);
  pinMode(PIN_G6, INPUT);
  pinMode(PIN_SCL, INPUT);
  pinMode(PIN_SDA, INPUT);
  pinMode(PIN_CIPO, INPUT);
  pinMode(PIN_COPI, INPUT);
  pinMode(PIN_SCK, INPUT);
  pinMode(PIN_D1, INPUT);
  pinMode(PIN_D0, INPUT);
  pinMode(PIN_PWM1, INPUT);
  pinMode(PIN_PWM0, INPUT);
  pinMode(PIN_A1, ANALOG);
  pinMode(PIN_A0, ANALOG);

  pinMode(LED_BUILTIN, OUTPUT);

  // Start serial port device
  Serial.begin(115200);
  Serial1.begin(115200);
  digitalWrite(LED_BUILTIN, HIGH);
  Wire.begin();
}

// This function polls fuel gauge for lipo voltage. 
// It will send the actual voltage preceedded by HDR_LIPO header (5 bytes total)
// or HDR_FUEL_GAUGE_ERR with padding zeros on error. 
void CheckLipo()
{
  uint8_t buffer[5] = {0};
  char temp[4] = {0};
  if (lipo.begin() == false) 
  {
    buffer[0] = HDR_FUEL_GAUGE_ERR;
    Serial1.write(buffer, 5);
    return;
  }

  lipo.quickStart();
  float voltage = lipo.getVoltage();
  sprintf(temp, "%.2f", voltage);
  memcpy(&buffer[1], temp, 4);
  buffer[0] = HDR_LIPO;
  Serial1.write(buffer, 5);
}

void loop()
{
  // clear input buffer
  memset(buffer, 0, 3);

  while (Serial1.available() < 3)
    delay(10);

  Serial1.readBytes(buffer, 3);

  // Parse header and call functions accordingly
  switch (buffer[0])
  {
  case HDR_ACK:
    break;

  case HDR_ALIVE:
    Serial1.write(HDR_ALIVE);
    break;

  case HDR_INPUT_PULLDOWN:
    SetPullDown();
    break;

  case HDR_INPUT_PULLUP:
    SetPullup();
    break;

  case HDR_READ_ANALOG:
    ReadAnalog();
    break;
  
  case HDR_READ_SD:
    ReadSDCard();
    break;

  case HDR_LIPO:
    CheckLipo();
    break;

  case HDR_GREETING:
    Serial.println(F("Hi! ESP32 here..."));
    break;

  }
}
