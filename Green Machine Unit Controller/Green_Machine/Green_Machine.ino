#include <FastLED.h>
#include <SoftwareSerial.h>
#include <MHZ.h>
#include <Wire.h>
#include <ArduCAM.h>
#include <SPI.h>
#include "DHT.h"
#include "memorysaver.h"
#include "cn0398.h"

#define tx 11
#define rx 10
SoftwareSerial Bluetooth(rx, tx);

// Camera
#if !(defined OV2640_MINI_2MP_PLUS)
  #error Please select the hardware platform and camera module in the ../libraries/ArduCAM/memorysaver.h file
#endif

#define BMPIMAGEOFFSET 66
const char bmp_header[BMPIMAGEOFFSET] PROGMEM =
{
  0x42, 0x4D, 0x36, 0x58, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x42, 0x00, 0x00, 0x00, 0x28, 0x00,
  0x00, 0x00, 0x40, 0x01, 0x00, 0x00, 0xF0, 0x00, 0x00, 0x00, 0x01, 0x00, 0x10, 0x00, 0x03, 0x00,
  0x00, 0x00, 0x00, 0x58, 0x02, 0x00, 0xC4, 0x0E, 0x00, 0x00, 0xC4, 0x0E, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF8, 0x00, 0x00, 0xE0, 0x07, 0x00, 0x00, 0x1F, 0x00,
  0x00, 0x00
};

// CO2
#define CO2_IN 23
#define MH_Z19_RX  15 
#define MH_Z19_TX  14 
MHZ co2(MH_Z19_RX, MH_Z19_TX, CO2_IN, MHZ19C);

// Light Intensity
#define Light_In A0

//Moisture analog
#define moisturePin A1

// LED
#define LED_PIN     24
#define LED_ON      22
#define NUM_LEDS    200
#define BRIGHTNESS  170
#define LED_TYPE    WS2811
#define COLOR_ORDER GRB
CRGB leds[NUM_LEDS];
#define UPDATES_PER_SECOND 100
CRGBPalette16 currentPalette;
TBlendType    currentBlending;
extern CRGBPalette16 myRedWhiteBluePalette;
extern const TProgmemPalette16 myRedWhiteBluePalette_p PROGMEM;

// Water and Fan
#define ENA_Water 2
#define ENB_Fan 3
#define IN1_Water 26
#define IN2_Water 27
#define IN3_Fan 28
#define IN4_Fan 29

// Temperature and Humidity
#define DHTPIN 4 
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// Cameras 
const int CS_Cam1 = 49;
const int CS_Cam2 = 50;
bool is_header = false;
int mode = 0;
uint8_t start_capture = 0;
bool CAM1_EXIST = false;
bool CAM2_EXIST = false;
bool stopMotion = false;

#if defined (OV2640_MINI_2MP_PLUS)
  ArduCAM myCAM1( OV2640, CS_Cam1 );
#else
  ArduCAM myCAM1( OV5642, CS_Cam1 );
#endif

#if defined (OV2640_MINI_2MP_PLUS)
  ArduCAM myCAM2( OV2640, CS_Cam2 );
#else
  ArduCAM myCAM2( OV5642, CS_Cam2 );
#endif

uint8_t read_fifo_burst(ArduCAM myCAM1);
uint8_t read_fifo_burst(ArduCAM myCAM2);

// Bluetooth
char command;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(38400);
  pinMode(CO2_IN, INPUT);
  delay(100);

  // Setting up the sensors
  
  Serial.println("MHZ 19C CO2 Sensor");
  if (co2.isPreHeating()) {
    Serial.print("Preheating");
    while (co2.isPreHeating()) {
      Serial.print(".");
      delay(5000);
    }
    Serial.println();
  }
  
  pinMode(LED_ON, OUTPUT);
  digitalWrite(LED_ON, LOW);
  pinMode(ENA_Water, OUTPUT);
  pinMode(ENB_Fan, OUTPUT);
  pinMode(IN1_Water, OUTPUT);
  pinMode(IN2_Water, OUTPUT);
  pinMode(IN3_Fan, OUTPUT);
  pinMode(IN4_Fan, OUTPUT);

  digitalWrite(IN1_Water, LOW);
  digitalWrite(IN2_Water, HIGH);
  digitalWrite(IN3_Fan, LOW);
  digitalWrite(IN4_Fan, HIGH);

  pinMode(DHTPIN, INPUT);
  digitalWrite(DHTPIN, HIGH);

  pinMode(Light_In, INPUT);
  digitalWrite(Light_In, HIGH);

  pinMode(moisturePin, INPUT);
  digitalWrite(moisturePin, HIGH);

  delay(3000); // power-up safety delay
  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(BRIGHTNESS);

  dht.begin();
  Serial.println("Set up Done");

  // Setting up Bluetooth
  Bluetooth.begin(38400);
  pinMode(tx, OUTPUT);
  pinMode(rx, INPUT);

  // Initiating the CN0398 board
  cn0398.setup();
  cn0398.init();
  Serial.println("Do you want to perform pH calibration? [Y/N]");
  char response;
  while (!(Serial.available() > 0));
  while (Serial.available() > 0){
    response = Serial.read();
    Serial.println();
  }
  
  if (response == 'y' || response == 'Y'){
    cn0398.calibrate_ph();
  }
  else{
    cn0398.use_nernst = true;
  }

  // Initiating the cameras
  
  uint8_t vid, pid;
  uint8_t temp;
  Wire.begin();
  
  pinMode(CS_Cam1, OUTPUT);
  digitalWrite(CS_Cam1, HIGH);
  pinMode(CS_Cam2, OUTPUT);
  digitalWrite(CS_Cam2, HIGH);
  SPI.begin();

  // Camera 1
  myCAM1.write_reg(0x07, 0x08);
  delay(100);
  myCAM1.write_reg(0x07, 0x00);
  delay(100);
  //Check if the 4 ArduCAM Mini 5MP PLus Cameras' SPI bus is OK
  while (1) {
    myCAM1.write_reg(ARDUCHIP_TEST1, 0x55);
    temp = myCAM1.read_reg(ARDUCHIP_TEST1);
    if (temp != 0x55)
    {
      Serial.println(F("SPI1 interface Error!"));
    } else {
      CAM1_EXIST = true;
      Serial.println(F("SPI1 interface OK."));
    }

    if (!(CAM1_EXIST)) {
      delay(1000); continue;
    } else
      break;
  }

  while (1) {
    //Check if the camera module type is OV5642
    myCAM1.wrSensorReg8_8(0xff, 0x01);
    myCAM1.rdSensorReg8_8(OV2640_CHIPID_HIGH, &vid);
    myCAM1.rdSensorReg8_8(OV2640_CHIPID_LOW, &pid);
    if ((vid != 0x26) && ((pid != 0x41) || (pid != 0x42))) {
      Serial.println(F("Can't find OV2640 module!"));
      delay(1000); continue;
    } else {
      Serial.println(F("OV2640 detected.")); break;
    }
  }

  //Change to JPEG capture mode and initialize the OV5640 module
  myCAM1.set_format(JPEG);
  myCAM1.InitCAM();
  myCAM1.OV2640_set_JPEG_size(OV2640_1600x1200); delay(1000);
  delay(1000);
  myCAM1.clear_fifo_flag();
  myCAM1.write_reg(ARDUCHIP_FRAMES, FRAMES_NUM);

  myCAM1.clear_fifo_flag();
  Serial.println("Camera 1 Ready");

  // Camera 2
  myCAM2.write_reg(0x07, 0x08);
  delay(100);
  myCAM2.write_reg(0x07, 0x00);
  delay(100);
  //Check if the 4 ArduCAM Mini 5MP PLus Cameras' SPI bus is OK
  while (1) {
    myCAM2.write_reg(ARDUCHIP_TEST1, 0x55);
    temp = myCAM2.read_reg(ARDUCHIP_TEST1);
    if (temp != 0x55)
    {
      Serial.println(F("SPI1 interface Error!"));
    } else {
      CAM2_EXIST = true;
      Serial.println(F("SPI1 interface OK."));
    }

    if (!(CAM2_EXIST)) {
      delay(1000); continue;
    } else
      break;
  }

  while (1) {
    //Check if the camera module type is OV5642
    myCAM2.wrSensorReg8_8(0xff, 0x01);
    myCAM2.rdSensorReg8_8(OV2640_CHIPID_HIGH, &vid);
    myCAM2.rdSensorReg8_8(OV2640_CHIPID_LOW, &pid);
    if ((vid != 0x26) && ((pid != 0x41) || (pid != 0x42))) {
      Serial.println(F("Can't find OV2640 module!"));
      delay(1000); continue;
    } else {
      Serial.println(F("OV2640 detected.")); break;
    }
  }

  //Change to JPEG capture mode and initialize the OV5640 module
  myCAM2.set_format(JPEG);
  myCAM2.InitCAM();
  myCAM2.OV2640_set_JPEG_size(OV2640_1600x1200); delay(1000);
  delay(1000);
  myCAM2.clear_fifo_flag();
  myCAM2.write_reg(ARDUCHIP_FRAMES, FRAMES_NUM);

  myCAM2.clear_fifo_flag();
  Serial.println("Camera 2 Ready");
}

void loop()
{
  if (Bluetooth.available()){
    command = (char)Bluetooth.read();
    Serial.println(command);
    Bluetooth.println(command); 
  }

  if (command == 'l' || command == 'L'){
    digitalWrite(LED_ON, HIGH);
    char mode;
    while (!Bluetooth.available());
    mode = Bluetooth.read();
    int rgb[] = {255,255,255};

    if (mode == '1') {
      rgb[0] = 214;
      rgb[1] = 5;
      rgb[2] = 30;
      Bluetooth.println("Magenta");
      Serial.println("Magenta");
    }
    if (mode == '2'){
      rgb[0] = 107;
      rgb[1] = 106;
      rgb[2] = 38;
      Bluetooth.println("Redish Yellow");
      Serial.println("Redish Yellow");
    }
    LED(rgb[0],rgb[1],rgb[2]);
  }
  
  if (command == 'd' || command == 'D'){ // LED Turn off
    digitalWrite(LED_ON, LOW);
    digitalWrite(LED_PIN, LOW);
  }

  else if (command == 'c' || command == 'C'){
    int co2_ppm = read_co2();
    delay(100);
  }

  else if (command == 'w' || command == 'W'){
    int duration;
    enable_water(60);
  }

  else if (command == 's' || command == 'S'){
    disable_water();
  }

  else if (command == 'f' || command == 'F'){
    enable_fan();
  }

  else if (command == 'x' || command == 'X'){
    disable_fan();
  }

  else if (command == 'h' || command == 'H'){
    float humidity;
    humidity = read_humidity();
    delay(100);
  }

  else if (command == 't' || command == 'T'){
    float env_temp;
    env_temp = read_env_temp();
    delay(100);
  }

  else if (command == 'i' || command == 'I'){
    int intensity = read_light_intensity();
    delay(100);
  }

   else if (command == 'm' || command == 'M'){
    int moisture = read_moisture_analog();
    delay(100);
  }

  else if (command == 'p' || command == 'P'){
    float pH = read_ph();
    delay(100);
    Bluetooth.print("pH of soil: ");
    Bluetooth.println(pH);
  }

  else if (command == 'a' || command == 'A'){
    float soil_temp = read_rtd();
    delay(100);
    Bluetooth.print("Temperature of soil: ");
    Bluetooth.println(soil_temp);
  }

  else if (command == 'j' || command == 'J'){
    temp = 0xff;
    start_capture = 1;
    myCAM1.flush_fifo();
    myCAM1.clear_fifo_flag();
    myCAM1.start_capture();
    start_capture = 0;
    if (myCAM1.get_bit(ARDUCHIP_TRIG, CAP_DONE_MASK))
    {
      Serial.println(F("CAM1 Capture Done"));
      delay(50);
      read_fifo_burst(myCAM1);
      //Clear the capture done flag
      myCAM1.clear_fifo_flag();
    }
  }

  else if (command == 'k' || command == 'K'){
    temp = 0xff;
    start_capture = 1;
    myCAM2.flush_fifo();
    myCAM2.clear_fifo_flag();
    myCAM2.start_capture();
    start_capture = 0;
    if (myCAM2.get_bit(ARDUCHIP_TRIG, CAP_DONE_MASK))
    {
      Serial.println(F("CAM2 Capture Done"));
      delay(50);
      read_fifo_burst(myCAM2);
      //Clear the capture done flag
      myCAM2.clear_fifo_flag();
    }
  }

  command = NULL;
}

// Support functions for reading cameras and sensors
uint8_t read_fifo_burst(ArduCAM myCAM)
{
  uint8_t temp = 0, temp_last = 0;
  uint32_t length = 0;
  length = myCAM.read_fifo_length();
  Serial.println(length, DEC);
  if (length >= MAX_FIFO_SIZE) //512 kb
  {
    Serial.println(F("ACK CMD Over size. END"));
    return 0;
  }
  if (length == 0 ) //0 kb
  {
    Serial.println(F("ACK CMD Size is 0. END"));
    return 0;
  }
  myCAM.CS_LOW();
  myCAM.set_fifo_burst();//Set fifo burst mode
  temp =  SPI.transfer(0x00);
  length --;
  while ( length-- )
  {
    temp_last = temp;
    temp =  SPI.transfer(0x00);
    if (is_header == true)
    {
      Serial.write(temp);
    }
    else if ((temp == 0xD8) & (temp_last == 0xFF))
    {
      is_header = true;
      Serial.println(F("ACK IMG END"));
      Serial.write(temp_last);
      Serial.write(temp);
    }
    if ( (temp == 0xD9) && (temp_last == 0xFF) ) //If find the end ,break while,
    break;
    delayMicroseconds(15);
  }
  myCAM.CS_HIGH();
  is_header = false;
  return 1;
}

void LED(uint8_t red, uint8_t green, uint8_t blue){
  for(int i = 0; i < NUM_LEDS; ++i) {
     leds[i] = CRGB(red,green,blue);
     FastLED.show();
  }
}

int read_co2(void){
  // Read the CO2 sensor and return the concentration in ppm
  int ppm_pwm = co2.readCO2PWM();
  Bluetooth.print("CO2 in PPM: ");
  Bluetooth.println(ppm_pwm);
  return ppm_pwm;
}
  
void enable_water(int sec){
  // Supply water to plants for a duration of given seconds
  analogWrite(ENA_Water,255);
  Bluetooth.println("Water enabled");
}

void disable_water(void){
  analogWrite(ENA_Water,0);
  Bluetooth.println("Water disabled");
}

void enable_fan(void){
  // Turn on the fan for a duration of given seconds
  analogWrite(ENB_Fan,255);
  Bluetooth.println("Fan enabled");
}

void disable_fan(void){
  analogWrite(ENB_Fan,0);
  Bluetooth.println("Fan disabled");
}

float read_humidity(void){
  float h = dht.readHumidity();
  Bluetooth.print("Humidity = ");
  Bluetooth.println(h);
  return h;
}

float read_env_temp(void){
  float t = dht.readTemperature();
  Bluetooth.print("Environmental Temperature = ");
  Bluetooth.println(t);
  return t;
}

int read_light_intensity(void){
  int intensity = analogRead(Light_In);
  intensity = map(intensity, 0, 1024, 300, 1100);
  Bluetooth.print("Light Intensity = ");
  Bluetooth.println(intensity);
  return intensity;
}

int read_moisture_analog(void){
  int sensorValue = analogRead(moisturePin); 
  Bluetooth.print("Moisture of soil: ");
  Bluetooth.println(sensorValue);
  return sensorValue;
}

float read_rtd();
float read_ph(float temperature = 25.0);
float read_moisture();
