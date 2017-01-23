// Code to acquire acceleration data and/or ADC data and transmitt it via
// Bluetooth.
//
// Software is distributed under the MIT License, see Firmware_License.txt
// for more details.

// Software serial port for the Bluetooth communications
#include <SoftwareSerial.h>
SoftwareSerial BTSerial(10, 11); // RX | TX

// I2Cdev and MPU6050 must be installed as libraries, or else the .cpp/.h files
// for both classes must be in the include path of your project
#include "I2Cdev.h"
#include "MPU6050.h"


MPU6050 mpu;

// Arduino Wire library is required if I2Cdev I2CDEV_ARDUINO_WIRE implementation
// is used in I2Cdev.h
#if I2CDEV_IMPLEMENTATION == I2CDEV_ARDUINO_WIRE
    #include "Wire.h"
#endif

// class default I2C address is 0x68
// specific I2C addresses may be passed as a parameter here
// AD0 low = 0x68 (default for InvenSense evaluation board)
// AD0 high = 0x69
MPU6050 accelgyro;

// uncomment "OUTPUT_READABLE_ACCELGYRO" if you want to see a tab-separated
// list of the accel X/Y/Z and then gyro X/Y/Z values in decimal. Easy to read,
// not so easy to parse, and slow(er) over UART.
//#define OUTPUT_READABLE_ACCELGYRO

// uncomment "OUTPUT_BINARY_ACCELGYRO" to send all 6 axes of data as 16-bit
// binary, one right after the other. This is very fast (as fast as possible
// without compression or data loss), and easy to parse, but impossible to read
// for a human.
#define OUTPUT_BINARY_ACCELGYRO


#define LED_PIN 13
#define BUFFSIZE 64
#define RM_WINDOW_SIZE 15
#define RM_PK_WINDOW_SIZE 11

// Globals
byte btTemp = 0;
int16_t iX_Accel;
int16_t iY_Accel;
int16_t iZ_Accel;
int16_t iX_Gyro;
int16_t iY_Gyro;
int16_t iZ_Gyro;
int16_t iADC;
unsigned int iuTemp;
int16_t YAccel[BUFFSIZE];
int16_t ZAccel[BUFFSIZE];
int16_t YAccel_rm[BUFFSIZE];
int16_t ZAccel_rm[BUFFSIZE];
int16_t YAccel_rm_pk[BUFFSIZE];
int16_t ZAccel_rm_pk[BUFFSIZE];
int idxData;
int idxLast;
float fRMScale = 1 / RM_WINDOW_SIZE;

// Setup, runs once
void setup() 
{
    // join I2C bus (I2Cdev library doesn't do this automatically)
    #if I2CDEV_IMPLEMENTATION == I2CDEV_ARDUINO_WIRE
        Wire.begin();
    #elif I2CDEV_IMPLEMENTATION == I2CDEV_BUILTIN_FASTWIRE
        Fastwire::setup(400, true);
    #endif
    
    // initialize serial communication over the USB port
    Serial.begin(112500);
    delay(50);
    
    // initialize device
    Serial.println("Initializing I2C devices...");
    accelgyro.initialize();

    // verify connection
    Serial.println("Testing device connections...");
    Serial.println(accelgyro.testConnection() ? "MPU6050 connection successful" : "MPU6050 connection failed");

    // Reference: http://playground.arduino.cc/Main/MPU-6050
    // Configure the gyro and accel for a full scale range of 2 g's and
    // a bandwidth of 98 hertz, less than half of our sampling frequency.
    accelgyro.setDLPFMode(MPU6050_DLPF_BW_98);
    accelgyro.setFullScaleAccelRange(MPU6050_ACCEL_FS_2);
    
    Serial.flush();

    // configure Arduino LED for watch dog
    pinMode(LED_PIN, OUTPUT);

    // configure the ADC input pin
    pinMode(A0, INPUT);
    
    // Set timer0 interrupt.
    TCCR0A = 0;   // Set entire TCCR0A register to 0
    TCCR0B = 0;   // Same for TCCR0B

    // See "ISR Frequency Ranges.xlsx" for details
    OCR0A = 249;  
    
    // Turn on the CTC mode
    TCCR0A |= (1 << WGM01);
    // Set CS02, CS01 and CS00 bits for 256 prescaler
    TCCR0B |= (1 << CS02 );
    // Enable the timer compare interrupt
    TIMSK0 |= ( 1 << OCIE0A );

    // Initialize the index and data buffer
    idxData = 0;
    idxLast = 0;
    for (int idx = 0; idx < BUFFSIZE ; ++idx){
      YAccel[idx] = 0;
      ZAccel[idx] = 0;
      YAccel_rm[idx] = 0;
      ZAccel_rm[idx] = 0;
      YAccel_rm_pk[idx] = 0;
      ZAccel_rm_pk[idx] = 0;
    }
    
    // enable interrupts
    sei();

   
}

// Usually do the function, but all of the event
// is handled in the ISR function because
// function are time sensitive.
void loop(void) {
}

// Fire the loop
ISR(TIMER0_COMPA_vect){

  // The IC2 requires interrupts to be enabled so I've done there here. There is risk,
  // if the sampling frequency is high one interrupt can be called before another is 
  // complete and you get a race condition. I need precise timing for the signal 
  // processing in the remote device so I took the risk.
  sei();

  // Get the value from the MPU-6050 accelerometer and gyro
  iADC = analogRead(A0);
  mpu.getMotion6(&iX_Accel, &iY_Accel, &iZ_Accel, &iX_Gyro, &iY_Gyro, &iZ_Gyro);
  YAccel[idxData] = iY_Accel;
  ZAccel[idxData] = iZ_Accel;

  idxLast = mod((idxData-1), BUFFSIZE);
  YAccel_rm[idxData] = 0;
  Serial.print(idxData);
  Serial.print('|');
  Serial.println(idxLast);

  // Watchdog pin
  digitalWrite(LED_PIN, !digitalRead(LED_PIN)); 

  // Increment the counter
  idxData = ++idxData % BUFFSIZE;
  
}

// Our operand can be negative
int mod(int x, int m) {
    int r = x%m;
    return r<0 ? r+m : r;
}



