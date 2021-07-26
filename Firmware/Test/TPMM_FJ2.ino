#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include "SparkFun_Flying_Jalapeno_2_Arduino_Library.h"

// Pins definitions
#define I2C_EN 22
#define SERIAL_EN 23
#define SPI_EN 24
#define USB_EN 26
#define SDCARD_PWR_EN 30
#define SDCARD_EN 47
#define SDCARD_nCS 49
#define CIPO 50
#define COPI 51
#define SCK 52
#define CS 53
#define MICROMOD_D1 2
#define MICROMOD_G3 3
#define MICROMOD_G4 5
#define MICROMOD_G5 6
#define MICROMOD_SCK 7
#define MICROMOD_COPI 8
#define MICROMOD_G6 14
#define MICROMOD_CIPO 15
#define MICROMOD_nBOOT 17
#define MICROMOD_3V3 A1
#define MICROMOD_QWIIC_PWR A2
#define MICROMOD_nRST A3
#define MICROMOD_EN A4
#define MICROMOD_A0 A5
#define MICROMOD_A1 A6
#define MICROMOD_PWM0 A7
#define MICROMOD_G0 A8
#define MICROMOD_PWM1 A9
#define MICROMOD_G1 A10
#define MICROMOD_D0 A11
#define MICROMOD_G2 A12
#define MICROMOD_SDA 20
#define MICROMOD_SCL 21
#define MICROMOD_QWIIC_PWR_EN 67

// Global macros definitions
#define TIMEOUT_MSEC 2000

// Board version - uncomment for using V10
#define V11

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

// FJ2 instance
FlyingJalapeno2 FJ2(FJ2_STAT_LED, 3.3);

// Global variables
uint8_t buffer[4] = {0};
uint8_t textBuffer[20] = {0};

// This function will check if fuel gauge responds for I2C commands sent
// by FJ2. It also checks if SDA or SCL pins are shorted to ground.
// Returns true if all tests are OK or false otherwise.
bool I2CCheck()
{
  if (digitalRead(MICROMOD_SDA) == LOW)
  {
    Serial.println(F("Step 7: FAIL! I2C SDA line shorted to ground."));
    return false;
  }

  if (digitalRead(MICROMOD_SCL) == LOW)
  {
    Serial.println(F("Step 7: FAIL! I2C SCL line shorted to ground."));
    return false;
  }

  if (FJ2.verifyI2Cdevice(0x36))
  {
    Serial.println(F("Step 7: Fuel gauge found at I2C address 0x36."));
  }
  else
  {
    Serial.println(F("Step 7: FAIL! Fuel gauge was not found at I2C address 0x36."));
    return false;
  }
  return true;
}

// Function called when a board passes all tests. It will disable both
// power supplies and set all FJ2 pins to HighZ.
void EndTest()
{
  digitalWrite(FJ2_STAT_LED, LOW);
  SetPinsHighZ();
  Serial.println();
  SetPinsHighZ();
  FJ2.disableV1();
  FJ2.disableV2();
  Serial.println(F("\n*************** Board is OK! ***************"));
  Serial.println(F("If you need to test another board, please disconnect the battery and replace the board."));
  digitalWrite(FJ2_LED_TEST_PASS, HIGH);
}

// This function checks for all power rails for correct voltages. It also checks if TPMM 3.3V rail
// is within the correct range either using USB and with simulated battery.
// Returns true if all tests are OK or false otherwise.
bool PowerChecks()
{
  int analogMeasure = 0;
  Serial.println(F("Performing power buses voltage checks."));
  //Check VCC (by reading the 3.3V zener connected to A0 on the FJ2)
  if (!FJ2.testVCC())
  {
    Serial.println(F("Step 1: FAIL! Change FJ2 switch to 3.3V."));
    return false;
  }
  else
    Serial.println(F("Step 1: VCC is correctly set to 3.3V."));

  // Set VUSB as 5V, power it up and see if there's no short in the board
  FJ2.setVoltageV1(5.0);
  if (FJ2.isV1Shorted())
  {
    Serial.println(F("Step 2: FAIL! VUSB bus is shorted in ThingPlus MicroMod board."));
    return false;
  }
  else
    Serial.println(F("Step 2: VUSB bus is OK."));

  // Set VBAT as 4.2V, power it up and see if there's no short in the board
  FJ2.setVoltageV2(4.2);
  if (FJ2.isV2Shorted())
  {
    Serial.println(F("Step 3: FAIL! VBAT bus is shorted in ThingPlus MicroMod board."));
    return false;
  }
  else
    Serial.println(F("Step 3: VBAT bus is OK."));

  // Power up the board using VUSB
  FJ2.enableV1();
  delay(100);

  // Measure board's 3.3V rail
  analogMeasure = analogRead(MICROMOD_3V3);
  // Any values equals to or above 95% ov 3.3V (3.135V) will be acceptable.
  // Since the A/D reference is 3.3V, 3.135V will be (1023 * 3.135/3.3) or approx 972
  if (analogMeasure < 972)
  {
    Serial.print(F("Step 4: FAIL! 3.3V rail with USB power is below the minimum recommended value. Approx. bus voltage is: "));
    Serial.print(float(analogMeasure * 3.3f / 1023.f));
    Serial.print(F(" volts or raw value of "));
    Serial.print(analogMeasure);
    Serial.println(F("."));
    return false;
  }
  else
  {
    Serial.print(F("Step 4: 3.3V rail with USB power is OK. Approx. bus voltage is: "));
    Serial.print(float(analogMeasure * 3.3f / 1023.f));
    Serial.print(F(" volts or raw value of "));
    Serial.print(analogMeasure);
    Serial.println(F("."));
  }

  // Turn on the simulated VBAT bus power and turn off VUSB
  FJ2.enableV2();
  delay(100);
  FJ2.disableV1();

  // Measure board's 3.3V rail
  analogMeasure = analogRead(MICROMOD_3V3);
  // Any values equals to or above 95% ov 3.3V (3.135V) will be acceptable.
  // Since the A/D reference is 3.3V, 3.135V will be (1023 * 3.135/3.3) or approx 972
  if (analogMeasure < 972)
  {
    Serial.print(F("Step 5: FAIL! 3.3V rail with battery power is below the minimum recommended value. Approx. bus voltage is: "));
    Serial.print(float(analogMeasure * 3.3f / 1023.f));
    Serial.print(F(" volts or raw value of "));
    Serial.print(analogMeasure);
    Serial.println(F("."));
    return false;
  }
  else
  {
    Serial.print(F("Step 5: 3.3V rail with battery power is OK. Approx. bus voltage is: "));
    Serial.print(float(analogMeasure * 3.3f / 1023.f));
    Serial.print(F(" volts or raw value of "));
    Serial.print(analogMeasure);
    Serial.println(F("."));
  }

  // Returning power to VUSB
  FJ2.enableV1();
  // Turning off simulated VBAT bus
  FJ2.disableV2();

  analogMeasure = analogRead(MICROMOD_QWIIC_PWR);
  // Any values equals to or above 95% ov 3.3V (3.135V) will be acceptable.
  // Since the A/D reference is 3.3V, 3.135V will be (1023 * 3.135/3.3) or approx 972
  if (analogMeasure < 972)
  {
    Serial.print(F("Step 6: FAIL! QWIIC power is below the minimum recommended value. Approx. voltage is: "));
    Serial.print(float(analogMeasure * 3.3f / 1023.f));
    Serial.print(F(" volts or raw value of "));
    Serial.print(analogMeasure);
    Serial.println(F("."));
    return false;
  }
  else
  {
    Serial.print(F("Step 6: QWIIC power good. Approx. voltage is: "));
    Serial.print(float(analogMeasure * 3.3f / 1023.f));
    Serial.print(F(" volts or raw value of "));
    Serial.print(analogMeasure);
    Serial.println(F("."));
  }

  return true;
}

// Sets all FJ2 pins which interface with TPMM to HighZ
void SetPinsHighZ()
{
  pinMode(MICROMOD_D1, INPUT);
  pinMode(MICROMOD_G3, INPUT);
  pinMode(MICROMOD_G4, INPUT);
  pinMode(MICROMOD_G5, INPUT);
  pinMode(MICROMOD_SCK, INPUT);
  pinMode(MICROMOD_COPI, INPUT);
  pinMode(MICROMOD_G6, INPUT);
  pinMode(MICROMOD_CIPO, INPUT);
  pinMode(MICROMOD_A0, INPUT);
  pinMode(MICROMOD_A1, INPUT);
  pinMode(MICROMOD_PWM0, INPUT);
  pinMode(MICROMOD_G0, INPUT);
  pinMode(MICROMOD_PWM1, INPUT);
  pinMode(MICROMOD_G1, INPUT);
  pinMode(MICROMOD_D0, INPUT);
  pinMode(MICROMOD_G2, INPUT);
}

// Sends command to TPMM to set all pins as input pullup. Returns true if TPMM acknowledges the message correctly.
// Returns true if all tests are OK or false otherwise.
bool SendPullupCommand()
{
  Serial1.write(static_cast<uint8_t>(PACKET_HEADERS::HDR_INPUT_PULLUP));
  Serial1.write(0x0);
  Serial1.write(0x0);

  while (Serial1.available() < 3)
    delay(10);

  memset(buffer, 0, 3);
  Serial1.readBytes(buffer, 3);
  return buffer[0] == static_cast<uint8_t>(PACKET_HEADERS::HDR_INPUT_PULLUP);
}

// Sends command to TPMM to set all pins as input pulldown. Returns true if TPMM acknowledges the message correctly.
// Returns true if all tests are OK or false otherwise.
bool SendPulldownCommand()
{
  Serial1.write(static_cast<uint8_t>(PACKET_HEADERS::HDR_INPUT_PULLDOWN));
  Serial1.write(0x0);
  Serial1.write(0x0);

  while (Serial1.available() < 3)
    delay(10);

  memset(buffer, 0, 3);
  Serial1.readBytes(buffer, 3);
  return buffer[0] == static_cast<uint8_t>(PACKET_HEADERS::HDR_INPUT_PULLDOWN);
}

// Checks if the driving a specific pin is shorted with another one. 
// It will print out the offending pin connection.
// Returns true if all tests are OK or false otherwise.
bool CheckCrosstalk(uint8_t drivingPin, uint8_t driveMode)
{
  bool result = true;
  switch (drivingPin)
  {
  case MICROMOD_G0:
  {
    if (digitalRead(MICROMOD_G1) == driveMode)
    {
      Serial.println(F("\n--> G1 and G0 seem to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_G2) == driveMode)
    {
      Serial.println(F("\n--> G2 and G0 seems to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_G5) == driveMode)
    {
      Serial.println(F("\n--> G5 and G0 seem to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_G6) == driveMode)
    {
      Serial.println(F("\n--> G6 and G0 seem to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_CIPO) == driveMode)
    {
      Serial.println(F("\n--> CIPO and G0 seem to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_COPI) == driveMode)
    {
      Serial.println(F("\n--> COPI and G0 seem to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_SCK) == driveMode)
    {
      Serial.println(F("\n--> SCK and G0 seem to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_D1) == driveMode)
    {
      Serial.println(F("\n--> D1 and G0 seem to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_D0) == driveMode)
    {
      Serial.println(F("\n--> D0 and G0 seem to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_PWM1) == driveMode)
    {
      Serial.println(F("\n--> PWM1 and G0 seem to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_PWM0) == driveMode)
    {
      Serial.println(F("\n--> PWM0 and G0 seem to be shorted..."));
      result = false;
    }
  }
  break;

  case MICROMOD_G1:
  {
    if (digitalRead(MICROMOD_G0) == driveMode)
    {
      Serial.println(driveMode);
      Serial.println(F("\n--> G0 and G1 seem to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_G2) == driveMode)
    {
      Serial.println(F("\n--> G2 and G1 seems to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_G5) == driveMode)
    {
      Serial.println(F("\n--> G5 and G1 seem to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_G6) == driveMode)
    {
      Serial.println(F("\n--> G6 and G1 seem to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_CIPO) == driveMode)
    {
      Serial.println(F("\n--> CIPO and G1 seem to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_COPI) == driveMode)
    {
      Serial.println(F("\n--> COPI and G1 seem to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_SCK) == driveMode)
    {
      Serial.println(F("\n--> SCK and G1 seem to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_D1) == driveMode)
    {
      Serial.println(F("\n--> D1 and G1 seem to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_D0) == driveMode)
    {
      Serial.println(F("\n--> D0 and G1 seem to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_PWM1) == driveMode)
    {
      Serial.println(F("\n--> PWM1 and G1 seem to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_PWM0) == driveMode)
    {
      Serial.println(F("\n--> PWM0 and G1 seem to be shorted..."));
      result = false;
    }
  }
  break;

  case MICROMOD_G2:
  {
    if (digitalRead(MICROMOD_G0) == driveMode)
    {
      Serial.println(driveMode);
      Serial.println(F("\n--> G0 and G2 seem to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_G1) == driveMode)
    {
      Serial.println(F("\n--> G1 and G2 seems to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_G5) == driveMode)
    {
      Serial.println(F("\n--> G5 and G2 seem to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_G6) == driveMode)
    {
      Serial.println(F("\n--> G6 and G2 seem to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_CIPO) == driveMode)
    {
      Serial.println(F("\n--> CIPO and G2 seem to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_COPI) == driveMode)
    {
      Serial.println(F("\n--> COPI and G2 seem to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_SCK) == driveMode)
    {
      Serial.println(F("\n--> SCK and G2 seem to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_D1) == driveMode)
    {
      Serial.println(F("\n--> D1 and G2 seem to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_D0) == driveMode)
    {
      Serial.println(F("\n--> D0 and G2 seem to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_PWM1) == driveMode)
    {
      Serial.println(F("\n--> PWM1 and G2 seem to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_PWM0) == driveMode)
    {
      Serial.println(F("\n--> PWM0 and G2 seem to be shorted..."));
      result = false;
    }
  }
  break;

  case MICROMOD_G5:
  {
    if (digitalRead(MICROMOD_G0) == driveMode)
    {
      Serial.println(driveMode);
      Serial.println(F("\n--> G0 and G5 seem to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_G1) == driveMode)
    {
      Serial.println(F("\n--> G1 and G5 seems to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_G2) == driveMode)
    {
      Serial.println(F("\n--> G2 and G5 seem to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_G6) == driveMode)
    {
      Serial.println(F("\n--> G6 and G5 seem to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_CIPO) == driveMode)
    {
      Serial.println(F("\n--> CIPO and G5 seem to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_COPI) == driveMode)
    {
      Serial.println(F("\n--> COPI and G5 seem to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_SCK) == driveMode)
    {
      Serial.println(F("\n--> SCK and G5 seem to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_D1) == driveMode)
    {
      Serial.println(F("\n--> D1 and G5 seem to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_D0) == driveMode)
    {
      Serial.println(F("\n--> D0 and G5 seem to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_PWM1) == driveMode)
    {
      Serial.println(F("\n--> PWM1 and G5 seem to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_PWM0) == driveMode)
    {
      Serial.println(F("\n--> PWM0 and G5 seem to be shorted..."));
      result = false;
    }
  }
  break;

  case MICROMOD_G6:
  {
    if (digitalRead(MICROMOD_G0) == driveMode)
    {
      Serial.println(driveMode);
      Serial.println(F("\n--> G0 and G6 seem to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_G1) == driveMode)
    {
      Serial.println(F("\n--> G1 and G6 seems to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_G2) == driveMode)
    {
      Serial.println(F("\n--> G2 and G6 seem to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_G5) == driveMode)
    {
      Serial.println(F("\n--> G5 and G6 seem to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_CIPO) == driveMode)
    {
      Serial.println(F("\n--> CIPO and G6 seem to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_COPI) == driveMode)
    {
      Serial.println(F("\n--> COPI and G6 seem to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_SCK) == driveMode)
    {
      Serial.println(F("\n--> SCK and G6 seem to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_D1) == driveMode)
    {
      Serial.println(F("\n--> D1 and G6 seem to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_D0) == driveMode)
    {
      Serial.println(F("\n--> D0 and G6 seem to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_PWM1) == driveMode)
    {
      Serial.println(F("\n--> PWM1 and G6 seem to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_PWM0) == driveMode)
    {
      Serial.println(F("\n--> PWM0 and G6 seem to be shorted..."));
      result = false;
    }
  }
  break;

  case MICROMOD_CIPO:
  {
    if (digitalRead(MICROMOD_G0) == driveMode)
    {
      Serial.println(driveMode);
      Serial.println(F("\n--> G0 and CIPO seem to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_G1) == driveMode)
    {
      Serial.println(F("\n--> G1 and CIPO seems to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_G2) == driveMode)
    {
      Serial.println(F("\n--> G2 and CIPO seem to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_G5) == driveMode)
    {
      Serial.println(F("\n--> G5 and CIPO seem to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_G6) == driveMode)
    {
      Serial.println(F("\n--> G6 and CIPO seem to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_COPI) == driveMode)
    {
      Serial.println(F("\n--> COPI and CIPO seem to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_SCK) == driveMode)
    {
      Serial.println(F("\n--> SCK and CIPO seem to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_D1) == driveMode)
    {
      Serial.println(F("\n--> D1 and CIPO seem to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_D0) == driveMode)
    {
      Serial.println(F("\n--> D0 and CIPO seem to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_PWM1) == driveMode)
    {
      Serial.println(F("\n--> PWM1 and CIPO seem to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_PWM0) == driveMode)
    {
      Serial.println(F("\n--> PWM0 and CIPO seem to be shorted..."));
      result = false;
    }
  }
  break;

  case MICROMOD_COPI:
  {
    if (digitalRead(MICROMOD_G0) == driveMode)
    {
      Serial.println(driveMode);
      Serial.println(F("\n--> G0 and COPI seem to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_G1) == driveMode)
    {
      Serial.println(F("\n--> G1 and COPI seems to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_G2) == driveMode)
    {
      Serial.println(F("\n--> G2 and COPI seem to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_G5) == driveMode)
    {
      Serial.println(F("\n--> G5 and COPI seem to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_G6) == driveMode)
    {
      Serial.println(F("\n--> G6 and COPI seem to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_CIPO) == driveMode)
    {
      Serial.println(F("\n--> CIPO and COPI seem to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_SCK) == driveMode)
    {
      Serial.println(F("\n--> SCK and COPI seem to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_D1) == driveMode)
    {
      Serial.println(F("\n--> D1 and COPI seem to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_D0) == driveMode)
    {
      Serial.println(F("\n--> D0 and COPI seem to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_PWM1) == driveMode)
    {
      Serial.println(F("\n--> PWM1 and COPI seem to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_PWM0) == driveMode)
    {
      Serial.println(F("\n--> PWM0 and COPI seem to be shorted..."));
      result = false;
    }
  }
  break;

  case MICROMOD_SCK:
  {
    if (digitalRead(MICROMOD_G0) == driveMode)
    {
      Serial.println(driveMode);
      Serial.println(F("\n--> G0 and SCK seem to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_G1) == driveMode)
    {
      Serial.println(F("\n--> G1 and SCK seems to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_G2) == driveMode)
    {
      Serial.println(F("\n--> G2 and SCK seem to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_G5) == driveMode)
    {
      Serial.println(F("\n--> G5 and SCK seem to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_G6) == driveMode)
    {
      Serial.println(F("\n--> G6 and SCK seem to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_CIPO) == driveMode)
    {
      Serial.println(F("\n--> CIPO and SCK seem to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_COPI) == driveMode)
    {
      Serial.println(F("\n--> COPI and SCK seem to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_D1) == driveMode)
    {
      Serial.println(F("\n--> D1 and SCK seem to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_D0) == driveMode)
    {
      Serial.println(F("\n--> D0 and SCK seem to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_PWM1) == driveMode)
    {
      Serial.println(F("\n--> PWM1 and SCK seem to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_PWM0) == driveMode)
    {
      Serial.println(F("\n--> PWM0 and SCK seem to be shorted..."));
      result = false;
    }
  }
  break;

  case MICROMOD_D1:
  {
    if (digitalRead(MICROMOD_G0) == driveMode)
    {
      Serial.println(driveMode);
      Serial.println(F("\n--> G0 and D1 seem to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_G1) == driveMode)
    {
      Serial.println(F("\n--> G1 and D1 seems to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_G2) == driveMode)
    {
      Serial.println(F("\n--> G2 and D1 seem to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_G5) == driveMode)
    {
      Serial.println(F("\n--> G5 and D1 seem to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_G6) == driveMode)
    {
      Serial.println(F("\n--> G6 and D1 seem to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_CIPO) == driveMode)
    {
      Serial.println(F("\n--> CIPO and D1 seem to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_COPI) == driveMode)
    {
      Serial.println(F("\n--> COPI and D1 seem to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_SCK) == driveMode)
    {
      Serial.println(F("\n--> SCK and D1 seem to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_D0) == driveMode)
    {
      Serial.println(F("\n--> D0 and D1 seem to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_PWM1) == driveMode)
    {
      Serial.println(F("\n--> PWM1 and D1 seem to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_PWM0) == driveMode)
    {
      Serial.println(F("\n--> PWM0 and D1 seem to be shorted..."));
      result = false;
    }
  }
  break;

  case MICROMOD_D0:
  {
    if (digitalRead(MICROMOD_G0) == driveMode)
    {
      Serial.println(driveMode);
      Serial.println(F("\n--> G0 and D0 seem to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_G1) == driveMode)
    {
      Serial.println(F("\n--> G1 and D0 seems to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_G2) == driveMode)
    {
      Serial.println(F("\n--> G2 and D0 seem to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_G5) == driveMode)
    {
      Serial.println(F("\n--> G5 and D0 seem to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_G6) == driveMode)
    {
      Serial.println(F("\n--> G6 and D0 seem to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_CIPO) == driveMode)
    {
      Serial.println(F("\n--> CIPO and D0 seem to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_COPI) == driveMode)
    {
      Serial.println(F("\n--> COPI and D0 seem to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_SCK) == driveMode)
    {
      Serial.println(F("\n--> SCK and D0 seem to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_D1) == driveMode)
    {
      Serial.println(F("\n--> D1 and D0 seem to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_PWM1) == driveMode)
    {
      Serial.println(F("\n--> PWM1 and D0 seem to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_PWM0) == driveMode)
    {
      Serial.println(F("\n--> PWM0 and D0 seem to be shorted..."));
      result = false;
    }
  }
  break;

  case MICROMOD_PWM1:
  {
    if (digitalRead(MICROMOD_G0) == driveMode)
    {
      Serial.println(driveMode);
      Serial.println(F("\n--> G0 and PWM1 seem to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_G1) == driveMode)
    {
      Serial.println(F("\n--> G1 and PWM1 seems to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_G2) == driveMode)
    {
      Serial.println(F("\n--> G2 and PWM1 seem to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_G5) == driveMode)
    {
      Serial.println(F("\n--> G5 and PWM1 seem to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_G6) == driveMode)
    {
      Serial.println(F("\n--> G6 and PWM1 seem to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_CIPO) == driveMode)
    {
      Serial.println(F("\n--> CIPO and PWM1 seem to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_COPI) == driveMode)
    {
      Serial.println(F("\n--> COPI and PWM1 seem to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_SCK) == driveMode)
    {
      Serial.println(F("\n--> SCK and PWM1 seem to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_D1) == driveMode)
    {
      Serial.println(F("\n--> D1 and PWM1 seem to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_D0) == driveMode)
    {
      Serial.println(F("\n--> D0 and PWM1 seem to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_PWM0) == driveMode)
    {
      Serial.println(F("\n--> PWM0 and PWM1 seem to be shorted..."));
      result = false;
    }
  }
  break;

  case MICROMOD_PWM0:
  {
    if (digitalRead(MICROMOD_G0) == driveMode)
    {
      Serial.println(driveMode);
      Serial.println(F("\n--> G0 and PWM0 seem to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_G1) == driveMode)
    {
      Serial.println(F("\n--> G1 and PWM0 seems to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_G2) == driveMode)
    {
      Serial.println(F("\n--> G2 and PWM0 seem to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_G5) == driveMode)
    {
      Serial.println(F("\n--> G5 and PWM0 seem to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_G6) == driveMode)
    {
      Serial.println(F("\n--> G6 and PWM0 seem to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_CIPO) == driveMode)
    {
      Serial.println(F("\n--> CIPO and PWM0 seem to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_COPI) == driveMode)
    {
      Serial.println(F("\n--> COPI and PWM0 seem to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_SCK) == driveMode)
    {
      Serial.println(F("\n--> SCK and PWM0 seem to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_D1) == driveMode)
    {
      Serial.println(F("\n--> D1 and PWM0 seem to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_D0) == driveMode)
    {
      Serial.println(F("\n--> D0 and PWM0 seem to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_PWM1) == driveMode)
    {
      Serial.println(F("\n--> PWM1 and PWM0 seem to be shorted..."));
      result = false;
    }
  }
  break;

  default:
    break;
  }

  return result;
}

// Performs checks on digital signal pins.
// This functions uses CheckCrosstalk above.
// Returns true if all tests are OK or false otherwise.
bool DigitalChecks()
{
  // Clear input buffer
  memset(buffer, 0, 3);
  bool result;
  SetPinsHighZ();

  // G0 pin
  // High side test - G0 will be driven high while all neighboring pins will be set as input pulldown
  Serial.print(F("Step 11.1: Checking for crosstalk on pin G0..."));
  result = SendPulldownCommand();
  if (!result)
  {
    Serial.println(F("Step 11.1: FAIL! ThingPlus MicroMod did not reply to SendPullDownCommand accordingly."));
    return false;
  }
  // Set G0 as output and set it to HIGH
  pinMode(MICROMOD_G0, OUTPUT);
  digitalWrite(MICROMOD_G0, HIGH);
  result = CheckCrosstalk(MICROMOD_G0, HIGH);
  if (!result)
  {
    Serial.println(F("Step 11.1: FAIL! Check pin G0..."));
    return false;
  }

  result = SendPullupCommand();
  if (!result)
  {
    Serial.println(F("Step 11.1: FAIL! ThingPlus MicroMod did not reply to SendPullupCommand accordingly."));
    return false;
  }

  // Set G0 as output and set it to LOW
  pinMode(MICROMOD_G0, OUTPUT);
  digitalWrite(MICROMOD_G0, LOW);
  result = CheckCrosstalk(MICROMOD_G0, LOW);
  if (!result)
  {
    Serial.println(F("Step 11.1: FAIL! Check pin G0..."));
    return false;
  }
  Serial.println(F(" pass."));

  SetPinsHighZ();

  // G1 pin
  // High side test - G1 will be driven high while all neighboring pins will be set as input pulldown
  Serial.print(F("Step 11.2: Checking for crosstalk on pin G1..."));
  result = SendPulldownCommand();
  if (!result)
  {
    Serial.println(F("Step 11.2: FAIL! ThingPlus MicroMod did not reply to SendPullDownCommand accordingly."));
    return false;
  }
  // Set G1 as output and set it to HIGH
  pinMode(MICROMOD_G1, OUTPUT);
  digitalWrite(MICROMOD_G1, HIGH);
  result = CheckCrosstalk(MICROMOD_G1, HIGH);
  if (!result)
  {
    Serial.println(F("Step 11.2: FAIL! Check pin G1..."));
    return false;
  }

  result = SendPullupCommand();
  if (!result)
  {
    Serial.println(F("Step 11.2: FAIL! ThingPlus MicroMod did not reply to SendPullupCommand accordingly."));
    return false;
  }

  // Set G1 as output and set it to LOW
  pinMode(MICROMOD_G1, OUTPUT);
  digitalWrite(MICROMOD_G1, LOW);
  result = CheckCrosstalk(MICROMOD_G1, LOW);
  if (!result)
  {
    Serial.println(F("Step 11.2: FAIL! Check pin G1..."));
    return false;
  }
  Serial.println(F(" pass."));

  SetPinsHighZ();

  // G2 pin
  // High side test - G2 will be driven high while all neighboring pins will be set as input pulldown
  Serial.print(F("Step 11.3: Checking for crosstalk on pin G2..."));
  result = SendPulldownCommand();
  if (!result)
  {
    Serial.println(F("Step 11.3: FAIL! ThingPlus MicroMod did not reply to SendPullDownCommand accordingly."));
    return false;
  }
  // Set G2 as output and set it to HIGH
  pinMode(MICROMOD_G2, OUTPUT);
  digitalWrite(MICROMOD_G2, HIGH);
  result = CheckCrosstalk(MICROMOD_G2, HIGH);
  if (!result)
  {
    Serial.println(F("Step 11.3: FAIL! Check pin G2..."));
    return false;
  }

  result = SendPullupCommand();
  if (!result)
  {
    Serial.println(F("Step 11.3: FAIL! ThingPlus MicroMod did not reply to SendPullupCommand accordingly."));
    return false;
  }

  // Set G2 as output and set it to LOW
  pinMode(MICROMOD_G2, OUTPUT);
  digitalWrite(MICROMOD_G2, LOW);
  result = CheckCrosstalk(MICROMOD_G2, LOW);
  if (!result)
  {
    Serial.println(F("Step 11.3: FAIL! Check pin G2..."));
    return false;
  }
  Serial.println(F(" pass."));

  SetPinsHighZ();

  // G5 pin
  // High side test - G5 will be driven high while all neighboring pins will be set as input pulldown
  Serial.print(F("Step 11.4: Checking for crosstalk on pin G5..."));
  result = SendPulldownCommand();
  if (!result)
  {
    Serial.println(F("Step 11.4: FAIL! ThingPlus MicroMod did not reply to SendPullDownCommand accordingly."));
    return false;
  }
  // Set G5 as output and set it to HIGH
  pinMode(MICROMOD_G5, OUTPUT);
  digitalWrite(MICROMOD_G5, HIGH);
  result = CheckCrosstalk(MICROMOD_G5, HIGH);
  if (!result)
  {
    Serial.println(F("Step 11.4: FAIL! Check pin G5..."));
    return false;
  }

  result = SendPullupCommand();
  if (!result)
  {
    Serial.println(F("Step 11.4: FAIL! ThingPlus MicroMod did not reply to SendPullupCommand accordingly."));
    return false;
  }

  // Set G5 as output and set it to LOW
  pinMode(MICROMOD_G5, OUTPUT);
  digitalWrite(MICROMOD_G5, LOW);
  result = CheckCrosstalk(MICROMOD_G5, LOW);
  if (!result)
  {
    Serial.println(F("Step 11.4: FAIL! Check pin G5..."));
    return false;
  }
  Serial.println(F(" pass."));

  SetPinsHighZ();

  // G6 pin
  // High side test - G6 will be driven high while all neighboring pins will be set as input pulldown
  Serial.print(F("Step 11.5: Checking for crosstalk on pin G6..."));
  result = SendPulldownCommand();
  if (!result)
  {
    Serial.println(F("Step 11.5: FAIL! ThingPlus MicroMod did not reply to SendPullDownCommand accordingly."));
    return false;
  }
  // Set G6 as output and set it to HIGH
  pinMode(MICROMOD_G6, OUTPUT);
  digitalWrite(MICROMOD_G6, HIGH);
  result = CheckCrosstalk(MICROMOD_G6, HIGH);
  if (!result)
  {
    Serial.println(F("Step 11.5: FAIL! Check pin G6..."));
    return false;
  }

  result = SendPullupCommand();
  if (!result)
  {
    Serial.println(F("Step 11.5: FAIL! ThingPlus MicroMod did not reply to SendPullupCommand accordingly."));
    return false;
  }

  // Set G6 as output and set it to LOW
  pinMode(MICROMOD_G6, OUTPUT);
  digitalWrite(MICROMOD_G6, LOW);
  result = CheckCrosstalk(MICROMOD_G6, LOW);
  if (!result)
  {
    Serial.println(F("Step 11.5: FAIL! Check pin G6..."));
    return false;
  }
  Serial.println(F(" pass."));

  SetPinsHighZ();

  // CIPO pin
  // High side test - CIPO will be driven high while all neighboring pins will be set as input pulldown
  Serial.print(F("Step 11.6: Checking for crosstalk on pin CIPO..."));
  result = SendPulldownCommand();
  if (!result)
  {
    Serial.println(F("Step 11.6: FAIL! ThingPlus MicroMod did not reply to SendPullDownCommand accordingly."));
    return false;
  }
  // Set CIPO as output and set it to HIGH
  pinMode(MICROMOD_CIPO, OUTPUT);
  digitalWrite(MICROMOD_CIPO, HIGH);
  result = CheckCrosstalk(MICROMOD_CIPO, HIGH);
  if (!result)
  {
    Serial.println(F("Step 11.6: FAIL! Check pin CIPO..."));
    return false;
  }

  result = SendPullupCommand();
  if (!result)
  {
    Serial.println(F("Step 11.6: FAIL! ThingPlus MicroMod did not reply to SendPullupCommand accordingly."));
    return false;
  }

  // Set CIPO as output and set it to LOW
  pinMode(MICROMOD_CIPO, OUTPUT);
  digitalWrite(MICROMOD_CIPO, LOW);
  result = CheckCrosstalk(MICROMOD_CIPO, LOW);
  if (!result)
  {
    Serial.println(F("Step 11.6: FAIL! Check pin CIPO..."));
    return false;
  }
  Serial.println(F(" pass."));

  SetPinsHighZ();

  // COPI pin
  // High side test - COPI will be driven high while all neighboring pins will be set as input pulldown
  Serial.print(F("Step 11.7: Checking for crosstalk on pin COPI..."));
  result = SendPulldownCommand();
  if (!result)
  {
    Serial.println(F("Step 11.7: FAIL! ThingPlus MicroMod did not reply to SendPullDownCommand accordingly."));
    return false;
  }
  // Set COPI as output and set it to HIGH
  pinMode(MICROMOD_COPI, OUTPUT);
  digitalWrite(MICROMOD_COPI, HIGH);
  result = CheckCrosstalk(MICROMOD_COPI, HIGH);
  if (!result)
  {
    Serial.println(F("Step 11.7: FAIL! Check pin COPI..."));
    return false;
  }

  result = SendPullupCommand();
  if (!result)
  {
    Serial.println(F("Step 11.7: FAIL! ThingPlus MicroMod did not reply to SendPullupCommand accordingly."));
    return false;
  }

  // Set COPI as output and set it to LOW
  pinMode(MICROMOD_COPI, OUTPUT);
  digitalWrite(MICROMOD_COPI, LOW);
  result = CheckCrosstalk(MICROMOD_COPI, LOW);
  if (!result)
  {
    Serial.println(F("Step 11.7: FAIL! Check pin COPI..."));
    return false;
  }
  Serial.println(F(" pass."));

  SetPinsHighZ();

  // SCK pin
  // High side test - SCK will be driven high while all neighboring pins will be set as input pulldown
  Serial.print(F("Step 11.8: Checking for crosstalk on pin SCK..."));
  result = SendPulldownCommand();
  if (!result)
  {
    Serial.println(F("Step 11.8: FAIL! ThingPlus MicroMod did not reply to SendPullDownCommand accordingly."));
    return false;
  }
  // Set SCK as output and set it to HIGH
  pinMode(MICROMOD_SCK, OUTPUT);
  digitalWrite(MICROMOD_SCK, HIGH);
  result = CheckCrosstalk(MICROMOD_SCK, HIGH);
  if (!result)
  {
    Serial.println(F("Step 11.8: FAIL! Check pin SCK..."));
    return false;
  }

  result = SendPullupCommand();
  if (!result)
  {
    Serial.println(F("Step 11.8: FAIL! ThingPlus MicroMod did not reply to SendPullupCommand accordingly."));
    return false;
  }

  // Set SCK as output and set it to LOW
  pinMode(MICROMOD_SCK, OUTPUT);
  digitalWrite(MICROMOD_SCK, LOW);
  result = CheckCrosstalk(MICROMOD_SCK, LOW);
  if (!result)
  {
    Serial.println(F("Step 11.8: FAIL! Check pin SCK..."));
    return false;
  }
  Serial.println(F(" pass."));

  SetPinsHighZ();

  // D1 pin
  // High side test - D1 will be driven high while all neighboring pins will be set as input pulldown
  Serial.print(F("Step 11.9: Checking for crosstalk on pin D1..."));
  result = SendPulldownCommand();
  if (!result)
  {
    Serial.println(F("Step 11.9: FAIL! ThingPlus MicroMod did not reply to SendPullDownCommand accordingly."));
    return false;
  }
  // Set D1 as output and set it to HIGH
  pinMode(MICROMOD_D1, OUTPUT);
  digitalWrite(MICROMOD_D1, HIGH);
  result = CheckCrosstalk(MICROMOD_D1, HIGH);
  if (!result)
  {
    Serial.println(F("Step 11.9: FAIL! Check pin D1..."));
    return false;
  }

  result = SendPullupCommand();
  if (!result)
  {
    Serial.println(F("Step 11.9: FAIL! ThingPlus MicroMod did not reply to SendPullupCommand accordingly."));
    return false;
  }

  // Set D1 as output and set it to LOW
  pinMode(MICROMOD_D1, OUTPUT);
  digitalWrite(MICROMOD_D1, LOW);
  result = CheckCrosstalk(MICROMOD_D1, LOW);
  if (!result)
  {
    Serial.println(F("Step 11.9: FAIL! Check pin D1..."));
    return false;
  }
  Serial.println(F(" pass."));

  SetPinsHighZ();

  // D0 pin
  // High side test - D0 will be driven high while all neighboring pins will be set as input pulldown
  Serial.print(F("Step 11.10: Checking for crosstalk on pin D0..."));
  result = SendPulldownCommand();
  if (!result)
  {
    Serial.println(F("Step 11.10: FAIL! ThingPlus MicroMod did not reply to SendPullDownCommand accordingly."));
    return false;
  }
  // Set D0 as output and set it to HIGH
  pinMode(MICROMOD_D0, OUTPUT);
  digitalWrite(MICROMOD_D0, HIGH);
  result = CheckCrosstalk(MICROMOD_D0, HIGH);
  if (!result)
  {
    Serial.println(F("Step 11.10: FAIL! Check pin D0..."));
    return false;
  }

  result = SendPullupCommand();
  if (!result)
  {
    Serial.println(F("Step 11.10: FAIL! ThingPlus MicroMod did not reply to SendPullupCommand accordingly."));
    return false;
  }

  // Set D0 as output and set it to LOW
  pinMode(MICROMOD_D0, OUTPUT);
  digitalWrite(MICROMOD_D0, LOW);
  result = CheckCrosstalk(MICROMOD_D0, LOW);
  if (!result)
  {
    Serial.println(F("Step 11.10: FAIL! Check pin D0..."));
    return false;
  }
  Serial.println(F(" pass."));

  SetPinsHighZ();

  // PWM1 pin
  // High side test - PWM1 will be driven high while all neighboring pins will be set as input pulldown
  Serial.print(F("Step 11.11: Checking for crosstalk on pin PWM1..."));
  result = SendPulldownCommand();
  if (!result)
  {
    Serial.println(F("Step 11.11: FAIL! ThingPlus MicroMod did not reply to SendPullDownCommand accordingly."));
    return false;
  }
  // Set PWM1 as output and set it to HIGH
  pinMode(MICROMOD_PWM1, OUTPUT);
  digitalWrite(MICROMOD_PWM1, HIGH);
  result = CheckCrosstalk(MICROMOD_PWM1, HIGH);
  if (!result)
  {
    Serial.println(F("Step 11.11: FAIL! Check pin PWM1..."));
    return false;
  }

  result = SendPullupCommand();
  if (!result)
  {
    Serial.println(F("Step 11.11: FAIL! ThingPlus MicroMod did not reply to SendPullupCommand accordingly."));
    return false;
  }

  // Set PWM1 as output and set it to LOW
  pinMode(MICROMOD_PWM1, OUTPUT);
  digitalWrite(MICROMOD_PWM1, LOW);
  result = CheckCrosstalk(MICROMOD_PWM1, LOW);
  if (!result)
  {
    Serial.println(F("Step 11.11: FAIL! Check pin PWM1..."));
    return false;
  }
  Serial.println(F(" pass."));

  SetPinsHighZ();

  // PWM0 pin
  // High side test - PWM0 will be driven high while all neighboring pins will be set as input pulldown
  Serial.print(F("Step 11.12: Checking for crosstalk on pin PWM0..."));
  result = SendPulldownCommand();
  if (!result)
  {
    Serial.println(F("Step 11.12: FAIL! ThingPlus MicroMod did not reply to SendPullDownCommand accordingly."));
    return false;
  }
  // Set PWM0 as output and set it to HIGH
  pinMode(MICROMOD_PWM0, OUTPUT);
  digitalWrite(MICROMOD_PWM0, HIGH);
  result = CheckCrosstalk(MICROMOD_PWM0, HIGH);
  if (!result)
  {
    Serial.println(F("Step 11.12: FAIL! Check pin PWM0..."));
    return false;
  }

  result = SendPullupCommand();
  if (!result)
  {
    Serial.println(F("Step 11.12: FAIL! ThingPlus MicroMod did not reply to SendPullupCommand accordingly."));
    return false;
  }

  // Set PWM0 as output and set it to LOW
  pinMode(MICROMOD_PWM0, OUTPUT);
  digitalWrite(MICROMOD_PWM0, LOW);
  result = CheckCrosstalk(MICROMOD_PWM0, LOW);
  if (!result)
  {
    Serial.println(F("Step 11.12: FAIL! Check pin PWM0..."));
    return false;
  }
  Serial.println(F(" pass."));
  return true;
}

// Checks if pins are shorted to either GND or 3.3V.
// It will print out the offending pin.
// Returns true if all tests are OK or false otherwise.
bool PinToSupplyCheck()
{
  SetPinsHighZ();
  bool result = true;
  Serial.print(F("Step 9.1: Checking for pins shorted to ground..."));

  if (SendPullupCommand())
  {
    if (digitalRead(MICROMOD_G0) == LOW)
    {
      Serial.println(F("\n--> G0 seems to be grounded..."));
      result = false;
    }

    if (digitalRead(MICROMOD_G1) == LOW)
    {
      Serial.println(F("\n--> G1 seems to be grounded..."));
      result = false;
    }

    if (digitalRead(MICROMOD_G2) == LOW)
    {
      Serial.println(F("\n--> G2 and G0 seems to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_G5) == LOW)
    {
      Serial.println(F("\n--> G5 seems to be grounded..."));
      result = false;
    }

    if (digitalRead(MICROMOD_G6) == LOW)
    {
      Serial.println(F("\n--> G6 seems to be grounded..."));
      result = false;
    }

    if (digitalRead(MICROMOD_CIPO) == LOW)
    {
      Serial.println(F("\n--> CIPO seems to be grounded..."));
      result = false;
    }

    if (digitalRead(MICROMOD_COPI) == LOW)
    {
      Serial.println(F("\n--> COPI seems to be grounded..."));
      result = false;
    }

    if (digitalRead(MICROMOD_SCK) == LOW)
    {
      Serial.println(F("\n--> SCK seems to be grounded..."));
      result = false;
    }

    if (digitalRead(MICROMOD_D1) == LOW)
    {
      Serial.println(F("\n--> D1 seems to be grounded..."));
      result = false;
    }

    if (digitalRead(MICROMOD_D0) == LOW)
    {
      Serial.println(F("\n--> D0 seems to be grounded..."));
      result = false;
    }

    if (digitalRead(MICROMOD_PWM1) == LOW)
    {
      Serial.println(F("\n--> PWM1 seems to be grounded..."));
      result = false;
    }

    if (digitalRead(MICROMOD_PWM0) == LOW)
    {
      Serial.println(F("\n--> PWM0 seems to be grounded..."));
      result = false;
    }

    if (!result)
    {
      Serial.println(F("Step 9.1: FAIL! Some pins are grounded and they should not be."));
      return false;
    }
    else
    {
      Serial.println(F(" pass."));
    }
  }
  else
  {
    Serial.println(F("Step 9.1: FAIL! ThingPlus MicroMod did not reply to SendPullUpCommand accordingly."));
    return false;
  }

  Serial.print(F("Step 9.2: Checking for pins shorted to 3.3V..."));
  result = SendPulldownCommand();
  if (result)
  {
    if (digitalRead(MICROMOD_G0) == HIGH)
    {
      Serial.println(F("\n--> G0 seems to be shorted to 3.3V..."));
      result = false;
    }

    if (digitalRead(MICROMOD_G1) == HIGH)
    {
      Serial.println(F("\n--> G1 seems to be shorted to 3.3V..."));
      result = false;
    }

    if (digitalRead(MICROMOD_G2) == HIGH)
    {
      Serial.println(F("\n--> G2 and G0 seems to be shorted..."));
      result = false;
    }

    if (digitalRead(MICROMOD_G5) == HIGH)
    {
      Serial.println(F("\n--> G5 seems to be shorted to 3.3V..."));
      result = false;
    }

    if (digitalRead(MICROMOD_G6) == HIGH)
    {
      Serial.println(F("\n--> G6 seems to be shorted to 3.3V..."));
      result = false;
    }

    if (digitalRead(MICROMOD_CIPO) == HIGH)
    {
      Serial.println(F("\n--> CIPO seems to be shorted to 3.3V..."));
      result = false;
    }

    if (digitalRead(MICROMOD_COPI) == HIGH)
    {
      Serial.println(F("\n--> COPI seems to be shorted to 3.3V..."));
      result = false;
    }

    if (digitalRead(MICROMOD_SCK) == HIGH)
    {
      Serial.println(F("\n--> SCK seems to be shorted to 3.3V..."));
      result = false;
    }

    if (digitalRead(MICROMOD_D1) == HIGH)
    {
      Serial.println(F("\n--> D1 seems to be shorted to 3.3V..."));
      result = false;
    }

    if (digitalRead(MICROMOD_D0) == HIGH)
    {
      Serial.println(F("\n--> D0 seems to be shorted to 3.3V..."));
      result = false;
    }

    if (digitalRead(MICROMOD_PWM1) == HIGH)
    {
      Serial.println(F("\n--> PWM1 seems to be shorted to 3.3V..."));
      result = false;
    }

    if (digitalRead(MICROMOD_PWM0) == HIGH)
    {
      Serial.println(F("\n--> PWM0 seems to be shorted to 3.3V..."));
      result = false;
    }

    if (!result)
    {
      Serial.println(F("Step 9.2: FAIL! Some pins are shorted to 3.3V and they should not be."));
      return false;
    }
    else
    {
      Serial.println(F(" pass."));
    }
  }
  else
  {
    Serial.println(F("Step 9.2: FAIL! ThingPlus MicroMod did not reply to SendPullDownCommand accordingly."));
    return false;
  }
  return true;
}

// Performs checks on analog signal pins.
// Will do a simple low rail/high rail analog conversion and check for correctness.
// Returns true if all tests are OK or false otherwise.
bool AnalogPinsCheck()
{
  SetPinsHighZ();
  pinMode(MICROMOD_A0, OUTPUT);
  pinMode(MICROMOD_A1, OUTPUT);
  digitalWrite(MICROMOD_A0, LOW);
  digitalWrite(MICROMOD_A1, LOW);
  Serial.print(F("Step 10.1: Checking analog reading when driven to ground..."));
  Serial1.write(HDR_READ_ANALOG);
  Serial1.write(0x0);
  Serial1.write(0x0);
  while (Serial1.available() < 3)
    delay(10);

  memset(buffer, 0, 3);
  Serial1.readBytes(buffer, 3);

  bool result = (buffer[1] < 0x08) && (buffer[2] < 0x08);
  if (result)
  {
    Serial.println(F(" pass."));
  }
  else
  {
    if (buffer[1] >= 0x08)
      Serial.println(F("\n--> A0 driven low fail."));

    if (buffer[2] >= 0x08)
      Serial.println(F("\n--> A1 driven low fail."));

    Serial.println(F("Step 10.1: FAIL! ThingPlus MicroMod analog pins not reading properly when tied to ground."));
    return false;
  }

  digitalWrite(MICROMOD_A0, HIGH);
  digitalWrite(MICROMOD_A1, HIGH);
  Serial.print(F("Step 10.2: Checking analog reading when driven to 3.3V..."));
  Serial1.write(HDR_READ_ANALOG);
  Serial1.write(0x0);
  Serial1.write(0x0);
  while (Serial1.available() < 3)
    delay(10);

  memset(buffer, 0, 3);
  Serial1.readBytes(buffer, 3);

  result = (buffer[1] > 0xfa) && (buffer[2] > 0xfa);
  if (result)
  {
    Serial.println(F(" pass."));
  }
  else
  {
    if (buffer[1] <= 0xfa)
      Serial.println(F("\n--> A0 driven high fail."));

    if (buffer[2] <= 0xfa)
      Serial.println(F("\n--> A1 driven high fail."));

    Serial.println(F("Step 10.2: FAIL! ThingPlus MicroMod analog pins not reading properly when tied to 3.3V."));
    return false;
  }
  return true;
}

// This function reads the file test.txt in the SD card.
// The actual success message is read from the SD card.
// Returns true if all tests are OK or false otherwise.
bool SDCardCheck()
{
  //Serial.println("Please insert the SD card in the TPMM card socket and touch TEST.");
  Serial.print(F("Step 12.1: Checking SD card... "));

  while (Serial1.available())
    Serial1.read();

  Serial1.write(HDR_READ_SD);
  Serial1.write(0x0);
  Serial1.write(0x0);

  Serial1.readBytes(textBuffer, 20);

  if (textBuffer[0] == HDR_SD_DATA)
  {
    Serial.println((char *)&textBuffer[1]);
  }
  else
  {
    Serial.println(F("\nStep 12.1: FAIL! Read SD card failed."));
    return false;
  }
  return true;
}

// This function informs the technician to check for a greeting message on TPMM USB UART.
void UARTCheck()
{
  Serial.println(F("Step 13: Checking USB UART... Check for serial output in TPMM's USB serial port."));
  Serial1.write(HDR_GREETING);
  Serial1.write(0x0);
  Serial1.write(0x0);
}

// This function will check for if the fuel gauge is correctly reading the battery voltage.
// The reading is sent to FJ2 via serial from TPMM so a full I2C->FuelGauge->Battery connection check is made.
// Returns true if all tests are OK or false otherwise.
bool ChargerCheck()
{
  Serial.println(F("Step 14.1: Connect a LiPo battery to the board and touch TEST."));
  while (!FJ2.isTestPressed())
    ;

  Serial1.write(HDR_LIPO);
  Serial1.write(0x0);
  Serial1.write(0x0);

  while (Serial1.available() < 3)
    delay(5);

  memset(textBuffer, 0, 20);
  Serial1.readBytes(textBuffer, 20);

  if (textBuffer[0] == HDR_FUEL_GAUGE_ERR)
  {
    Serial.println(F("Step 14.1: FAIL! MAX fuel gauge not responding to ESP32 I2C."));
    return false;
  }

  Serial.print(F("Step 14.2: Voltage reading while charging is "));
  Serial.print((char *)&textBuffer[1]);
  Serial.println(F(" volts."));

  Serial.println(F("Step 14.3: Removing board power. Keep the battery connected."));
  FJ2.disableV1();

  Serial1.write(HDR_LIPO);
  Serial1.write(0x0);
  Serial1.write(0x0);

  uint64_t start = millis();
  while (Serial1.available() < 3)
  {
    if (millis() - start > TIMEOUT_MSEC)
    {
      Serial.print(F("Step 14.3: FAIL! Board was not powered up."));
      return false;
    }
    else
      delay(5);
  }

  memset(textBuffer, 0, 20);
  Serial1.readBytes(textBuffer, 20);

  Serial.print(F("Step 14.3: Battery voltage is "));
  Serial.print((char *)&textBuffer[1]);
  Serial.println(F(" volts."));

  return true;
}

void setup()
{ // Reset FJ2 but keep the LEDs on
  FJ2.reset(false);

  // Starting serial port
  Serial.begin(115200);
  Serial.println();
  Serial1.begin(115200);
  Serial.println(F("ThingPlus MicroMod test program - Version 1.0"));
  Serial.println(F("Beginning..."));
  FJ2.setCapSenseThreshold(4096);
  FJ2.enableI2CBuffer(); //Enable the I2C buffer
}

void loop()
{
  while (true)
  {
    digitalWrite(FJ2_LED_TEST_PASS, LOW);

    // Start I2C peripheral
    Wire.begin();

    // Pin setup
    pinMode(MICROMOD_nRST, OUTPUT);
    digitalWrite(MICROMOD_nRST, LOW);
    pinMode(I2C_EN, OUTPUT);
    digitalWrite(I2C_EN, HIGH);
    pinMode(SERIAL_EN, OUTPUT);

    #ifdef V11
    digitalWrite(SERIAL_EN, HIGH);
    #else
    digitalWrite(SERIAL_EN, LOW);
    #endif

    pinMode(USB_EN, OUTPUT);
    digitalWrite(USB_EN, HIGH);

    // Hold M.2 processor reset for now
    digitalWrite(FJ2_STAT_LED, HIGH);

    if (!PowerChecks())
      break;
    if (!I2CCheck())
      break;

    pinMode(MICROMOD_nRST, INPUT);
    delay(200);

    Wire.end();

    // Clear Serial1 buffer
    while (Serial1.available())
      Serial1.read();

    // Check if ThingPlus MicroMod is ready
    memset(buffer, 0, 3);
    buffer[0] = static_cast<uint8_t>(PACKET_HEADERS::HDR_ALIVE);
    Serial1.write(buffer, 3);

    unsigned long start = millis();
    unsigned long now = start;

    do
    {
      now = millis();
      if (now - start > TIMEOUT_MSEC)
      {
        Serial.println(F("Step 8: FAIL! ThingPlus serial is not responding."));
        Serial.println(F("Check for shorts or open circuits in TX/RX/G2/G3 lines."));
        break;
      }
    } while (Serial1.available() == 0);

    if (Serial1.read() != static_cast<uint8_t>(PACKET_HEADERS::HDR_ALIVE))
    {
      Serial.println(F("Step 8: FAIL! ThingPlus MicroMod serial reply was not HDR_ALIVE."));
      Serial.println(F("Check for shorts or open circuits in TX/RX/G2/G3 lines."));
      break;
    }

    Serial.println(F("Step 8: ThingPlus MicroMod serial communication is OK."));

    // Checks if pins are shorted to either GND or 3.3V.
    if (!PinToSupplyCheck())
      break;

    // Checks for analog pins functionality
    if (!AnalogPinsCheck())
      break;

    // Checks for digital pins functionality
    if (!DigitalChecks())
      break;

    // Checks for SD card reader functionality
    if (!SDCardCheck())
      break;

    // Checks TPMM USB UART functionality
    UARTCheck();

    // Checks for fuel gauge functionality
    if (!ChargerCheck())
      break;

    // If we reached this far the board is good. 
    EndTest();

    Serial.println(F("Replace board and touch TEST to restart the test."));
    while(!FJ2.isTestPressed());
  }

  // Ooops! Something is wrong with the board!
  // Warn the user, disable the board and wait for another round.
  digitalWrite(FJ2_STAT_LED, LOW);
  SetPinsHighZ();
  FJ2.disableV1();
  FJ2.disableV2();
  Serial.println();
  Serial.println(F("*************** Board test failed. ***************"));
  Serial.println(F("Touch PROGRAM AND TEST to reset FJ2..."));
  do
  {
    digitalWrite(FJ2_LED_FAIL, HIGH);
    delay(250);
    digitalWrite(FJ2_LED_FAIL, LOW);
    delay(250);
    if (FJ2.isProgramAndTestPressed())
      break;
  } while (true);
}
