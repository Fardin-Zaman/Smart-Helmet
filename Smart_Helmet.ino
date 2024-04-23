
#include <Wire.h>  // Wire library - used for I2C communication
#include <AltSoftSerial.h>
#include <TinyGPS++.h>
#include <SoftwareSerial.h>
#include <math.h>

const int buttonTimeout = 4000;  // Button timeout in milliseconds (1 minute)
const int thresholdTemp = 10;    // Temperature threshold in degrees Celsius
//const int debounceTime = 5000;        // Debounce time for button press in milliseconds
unsigned long lastButtonPress = 0;  // Stores the time of the last button press
unsigned long lastLedBlink = 0;     // Stores the time of the last LED blink

//--------------------------------------------------------------
//emergency phone number with country code
const String EMERGENCY_PHONE = "+916900595008";
//--------------------------------------------------------------
//GSM Module RX pin to Arduino 3
//GSM Module TX pin to Arduino 2
#define rxPin 2
#define txPin 3
SoftwareSerial sim800(rxPin, txPin);
//--------------------------------------------------------------
//GPS Module RX pin to Arduino 9
//GPS Module TX pin to Arduino 8
AltSoftSerial neogps;
TinyGPSPlus gps;
//--------------------------------------------------------------
String sms_status, sender_number, received_date, msg;
String latitude, longitude;
//--------------------------------------------------------------
#define BUZZER 12
#define BUTTON 4
//--------------------------------------------------------------
bool messageSent = false;
bool buzzerState = false;

//--------------------------------------------------------------

int ADXL345 = 0x53;  // The ADXL345 sensor I2C address

float X_out, Y_out, Z_out;  // Outputs

void setup() {

  //--------------------------------------------------------------
  //Serial.println("Arduino serial initialize");
  Serial.begin(9600);
  //--------------------------------------------------------------
  //Serial.println("SIM800L serial initialize");
  sim800.begin(9600);
  //--------------------------------------------------------------
  //Serial.println("NEO6M serial initialize");
  neogps.begin(9600);
  //--------------------------------------------------------------
  pinMode(BUZZER, OUTPUT);
  pinMode(BUTTON, INPUT_PULLUP);
  //--------------------------------------------------------------

  Wire.begin();
  Wire.beginTransmission(ADXL345);  // Start communicating with the device
  Wire.write(0x2D);                 // Access/ talk to POWER_CTL Register - 0x2D
  // Enable measurement
  Wire.write(8);  // Bit D3 High for measuring enable (8dec -> 0000 1000 binary)
  Wire.endTransmission();
  delay(10);

  //Off-set Calibration
  //X-axis
  Wire.beginTransmission(ADXL345);
  Wire.write(0x1E);
  Wire.write(-3);
  Wire.endTransmission();
  delay(3);
  //Y-axis
  Wire.beginTransmission(ADXL345);
  Wire.write(0x1F);
  Wire.write(2);
  Wire.endTransmission();
  delay(10);

  //Z-axis
  Wire.beginTransmission(ADXL345);
  Wire.write(0x20);
  Wire.write(-42);
  Wire.endTransmission();
  delay(10);
  //--------------------------------------------------------------------------
  //--------------------------------------------------------------
  sms_status = "";
  sender_number = "";
  received_date = "";
  msg = "";
  //--------------------------------------------------------------
  sim800.println("AT");  //Check GSM Module
  delay(1000);
  //SendAT("AT", "OK", 2000); //Check GSM Module
  sim800.println("ATE1");  //Echo ON
  delay(1000);
  //SendAT("ATE1", "OK", 2000); //Echo ON
  sim800.println("AT+CPIN?");  //Check SIM ready
  delay(1000);
  //SendAT("AT+CPIN?", "READY", 2000); //Check SIM ready
  sim800.println("AT+CMGF=1");  //SMS text mode
  delay(1000);
  //SendAT("AT+CMGF=1", "OK", 2000); //SMS text mode
  sim800.println("AT+CNMI=1,1,0,0,0");  /// Decides how newly arrived SMS should be handled
  delay(1000);

}

void loop() {

//--------------------------------------------------------------
  // === Read acceleromter data === //
  Wire.beginTransmission(ADXL345);
  Wire.write(0x32);  // Start with register 0x32 (ACCEL_XOUT_H)
  Wire.endTransmission(false);
  Wire.requestFrom(ADXL345, 6, true);        // Read 6 registers total, each axis value is stored in 2 registers
  X_out = (Wire.read() | Wire.read() << 8);  // X-axis value
  X_out = X_out / 32;                        //For a range of +-2g, we need to divide the raw values by 256, according to the datasheet
  Y_out = (Wire.read() | Wire.read() << 8);  // Y-axis value
  Y_out = Y_out / 32;
  Z_out = (Wire.read() | Wire.read() << 8);  // Z-axis value
  Z_out = Z_out / 32;


  Serial.print("Xa= ");
  Serial.print(X_out);
  Serial.print("   Ya= ");
  Serial.print(Y_out);
  Serial.print("   Za= ");
  Serial.println(Z_out);
  delay(100);

//--------------------------------------------------------------
// ************************LOGIC********************************
//--------------------------------------------------------------

  if (abs(X_out) && abs(Y_out) && abs(Z_out) > 10) 
  {

    digitalWrite(BUZZER, HIGH);  // Turn on buzzer

    for (int i = 0; i < 1000; i++) 
    {
      delay(1000);
      // button is pressed
      if (digitalRead(BUTTON) == LOW) 
      {
        digitalWrite(BUZZER, LOW);
        break;

      }
       else 
       {
        if (i > 5) 
        {
          digitalWrite(BUZZER, LOW);
          sendSms("My bike crashed, save my soul. My location is: http://maps.google.com/maps?q=loc:26.142606,91.659609");
          Serial.println("SMS sent");
          makeCall();
          Serial.println("CAll done");
          break;
        }
      }
    }
  }
}

/*****************************************************************************************
 * parseData() function
 *****************************************************************************************/
void parseData(String buff) {
  Serial.println(buff);

  unsigned int len, index;
  //--------------------------------------------------------------
  //Remove sent "AT Command" from the response string.
  index = buff.indexOf("\r");
  buff.remove(0, index + 2);
  buff.trim();
  
  if (buff != "OK") {
    //--------------------------------------------------------------
    index = buff.indexOf(":");
    String cmd = buff.substring(0, index);
    cmd.trim();

    buff.remove(0, index + 2);
    //Serial.println(buff);
    //--------------------------------------------------------------
    if (cmd == "+CMTI") {
      //get newly arrived memory location and store it in temp
      //temp = 4
      index = buff.indexOf(",");
      String temp = buff.substring(index + 1, buff.length());
      temp = "AT+CMGR=" + temp + "\r";
      //AT+CMGR=4 i.e. get message stored at memory location 4
      sim800.println(temp);
    }
    //--------------------------------------------------------------
    else if (cmd == "+CMGR") {
      //extractSms(buff);
      //Serial.println(buff.indexOf(EMERGENCY_PHONE));
      if (buff.indexOf(EMERGENCY_PHONE) > 1) {
        buff.toLowerCase();
        //Serial.println(buff.indexOf("get gps"));
        if (buff.indexOf("get gps") > 1) {
          getGps();
          String sms_data;
          sms_data = "GPS Location Data\r";
          sms_data += "http://maps.google.com/maps?q=loc:";
          sms_data += latitude + "," + longitude;

          sendSms(sms_data);
        }
      }
    }
    //--------------------------------------------------------------
  } else {
    
  }
  
}


/*****************************************************************************************
 * getGps() Function
*****************************************************************************************/
void getGps() {
  // Can take up to 60 seconds
  boolean newData = false;
  for (unsigned long start = millis(); millis() - start < 2000;) {
    while (neogps.available()) {
      if (gps.encode(neogps.read())) {
        newData = true;
        break;
      }
    }
  }

  if (newData)  //If newData is true
  {
    latitude = String(gps.location.lat(), 6);
    longitude = String(gps.location.lng(), 6);
    newData = false;
  } else {
    Serial.println("No GPS data is available");
    latitude = "";
    longitude = "";
  }

  Serial.print("Latitude= ");
  Serial.println(latitude);
  Serial.print("Lngitude= ");
  Serial.println(longitude);
}



/*****************************************************************************************
* sendAlert() function
*****************************************************************************************/
void sendAlert() {
  String sms_data;
  sms_data = "Accident Alert!!\r";
  sms_data += "http://maps.google.com/maps?q=loc:";
  sms_data += latitude + "," + longitude;

  sendSms(sms_data);
}




/*****************************************************************************************
* makeCall() function
*****************************************************************************************/
void makeCall() {
  Serial.println("calling....");
  sim800.println("ATD" + EMERGENCY_PHONE + ";");
  delay(10000);  //20 sec delay
  sim800.println("ATH");
  delay(1000);  //1 sec delay
}

/*****************************************************************************************
 * sendSms() function
 *****************************************************************************************/
// void sendSms(String text) {
//   //return;
//   sim800.print("AT+CMGF=1\r");
//   delay(100);
//   sim800.print("AT+CMGS=\"EMERGENCY_PHONE\""); // Remove the extra semicolon and double quotes
//   delay(100);
//   sim800.println(text);
//   delay(100);
//   sim800.println((char)26);  // ASCII code for ctrl-26
//   delay(100);
//   Serial.println("SMS Sent Successfully.");
// }

void sendSms(String text) {
  //return;
  sim800.print("AT+CMGF=1\r");
  delay(100);
  sim800.print("AT+CMGS=\"");     // Start the AT command for sending SMS
  sim800.print(EMERGENCY_PHONE);  // Concatenate the phone number
  sim800.print("\"\r");           // End the phone number and send message command
  delay(100);
  sim800.println(text);  // Send the message
  delay(100);
  sim800.println((char)26);  // End the message transmission
  delay(100);
  Serial.println("SMS Sent Successfully.");
}



/*****************************************************************************************
 * SendAT() function
 *****************************************************************************************/
boolean SendAT(String at_command, String expected_answer, unsigned int timeout) {

  uint8_t x = 0;
  boolean answer = 0;
  String response;
  unsigned long previous;

  //Clean the input buffer
  while (sim800.available() > 0) sim800.read();

  sim800.println(at_command);

  x = 0;
  previous = millis();

  //this loop waits for the answer with time out
  do {
    //if there are data in the UART input buffer, reads it and checks for the asnwer
    if (sim800.available() != 0) {
      response += sim800.read();
      x++;
      // check if the desired answer (OK) is in the response of the module
      if (response.indexOf(expected_answer) > 0) {
        answer = 1;
        break;
      }
    }
  } while ((answer == 0) && ((millis() - previous) < timeout));

  Serial.println(response);
  return answer;
}

/*****************************************************************************************
 * END
 *****************************************************************************************/




