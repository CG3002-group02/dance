#include <Wire.h>

#include <Arduino_FreeRTOS.h>
#include <semphr.h>

//#define DEBUGP   // Uncomment statement to enable debugging mode

// Print macros
#ifdef DEBUGP
#define DEBUG_BEGIN(x)      Serial.begin(x)
#define DEBUG_PRINT(x)      Serial.print(x)
#define DEBUG_PRINTLN(x)    Serial.println(x)
#define DEBUG_WRITE(...)    Serial.write(__VA_ARGS__)
#else
#define DEBUG_BEGIN(x)
#define DEBUG_PRINT(x)
#define DEBUG_PRINTLN(x)
#define DEBUG_WRITE(...)
#endif

// Fixed constants - DO NOT TOUCH.
#define START_STOP_BYTE 0x7E
#define ESCAPE_BYTE     0x7D
#define MPU             0x68  // I2C address of the MPU-6050
#define SFRAME_RR       0x00
#define SFRAME_RNR      0x02
#define SFRAME_REJ      0x01
const byte final2Bits_HFrame = 0x03;
const byte final2Bits_SFrame = 0x01;

// Constants that may be adjusted according to our needs
#define VOLT_DIVIDER      A0
#define VOUT              A2
#define NUM_GY521         3
#define BAUD_RATE         115200
#define BUFFER_SIZE       16
#define MIN_IFRAME_LENGTH 50
#define MAX_IFRAME_LENGTH 160                         // Calculated Max length is 154

#ifndef pdMSTOTICKS
#define pdMS_TO_TICKS( xTimeInMs ) ( ( TickType_t ) ( ( ( TickType_t ) ( xTimeInMs ) * ( TickType_t ) configTICK_RATE_HZ ) / ( TickType_t ) 1000 ) )
#endif
// Tick problems - runs actually 17-18ms = 55 Hz. Accounted for in ML algo
const TickType_t xDelay = pdMS_TO_TICKS(25);    // 25ms - Period of TaskReadSensors

unsigned long startTime;
uint8_t send_seq = 1, recv_seq = 0; // used for filling in control byte fields when sending frames
uint8_t lastSent = 0;                // keeps track of the current last frame put in buffer
uint8_t lastACK = BUFFER_SIZE - 1;   // keeps track of the last acknowledged frame by the RPi. Set to BUFFER_SIZE - 1 to account for first ACK case after handshake.
uint8_t freeBuffer = BUFFER_SIZE;   // if = 0, stop reading new values
char send_buf[BUFFER_SIZE][MAX_IFRAME_LENGTH];

uint8_t buf_idx = 0;

// FreeRTOS data structures
QueueHandle_t xSerialSendQueue;
SemaphoreHandle_t xSerialSemaphore;  // Allow only one task to access the serial port


void setup() {
/*  send_buf[0][0] = 'a'; send_buf[0][1] = 'a'; send_buf[0][2] = 'a'; send_buf[0][3] = 'a';
  send_buf[1][0] = 'b'; send_buf[1][1] = 'a'; send_buf[1][2] = 'b'; send_buf[1][3] = 'a';
  send_buf[2][0] = 'c'; send_buf[2][1] = 'a'; send_buf[2][2] = 'c'; send_buf[2][3] = 'a';
  send_buf[3][0] = 'd'; send_buf[3][1] = 'a'; send_buf[3][2] = 'd'; send_buf[3][3] = 'a';
  send_buf[4][0] = 'e'; send_buf[4][1] = 'a'; send_buf[4][2] = 'e'; send_buf[4][3] = 'a';
  send_buf[5][0] = 'f'; send_buf[5][1] = 'a'; send_buf[5][2] = 'f'; send_buf[5][3] = 'a';
*/
  pinMode(5, OUTPUT);
  pinMode(6, OUTPUT);
  pinMode(7, OUTPUT);
  pinMode(VOLT_DIVIDER, INPUT); // Voltage Divider output
  pinMode(VOUT, INPUT);         // INA169 VOut

  DEBUG_BEGIN(BAUD_RATE);  // PC Serial
  Serial3.begin(BAUD_RATE);  // RPi Serial
  DEBUG_PRINTLN("Ready");

  xSerialSendQueue = xQueueCreate(BUFFER_SIZE, sizeof(uint32_t));
  xSerialSemaphore = xSemaphoreCreateMutex();
  xSemaphoreGive(xSerialSemaphore);

  establishContact();
  setupSensors();

  xTaskCreate(TaskReadSensors, "Read sensors", 512, NULL, 4, NULL); // Highest priority
  xTaskCreate(TaskSend, "Serial send", 256, NULL, 3, NULL);
  xTaskCreate(TaskRecv, "Serial recv", 512, NULL, 2, NULL);         // Lower priority, just receives acks

  DEBUG_PRINTLN("Starting scheduler");
  vTaskStartScheduler();
}

// Handshake between Arduino and RPi
void establishContact() {
  bool handshake = false;
  bool expectStopByte = false;
  // msg[0] = START, msg[1] = recv_seq, msg[2] = frame, msg[3] = checkNum, msg[4] = checkNum, msg[5] = START_STOP_BYTE
  byte msg[6];
  byte i = -1;           // to fill buf
  while (!handshake) {
    if (Serial3.available() <= 0) {    // RPi Serial
      continue;
    }
    msg[++i] = Serial3.read();         // RPi Serial

    if (i == 0 && msg[i] != START_STOP_BYTE) { // If receiving first byte but is not START byte
      DEBUG_PRINTLN("Error, frame doesnt start with 0x7e");
      i = -1;
      expectStopByte = false;
      memset(msg, NULL, 1);
    }
    else if (msg[i] == START_STOP_BYTE && !expectStopByte) {  // If is receiving the START byte
      expectStopByte = true;                        // this is the START byte, the next such byte should be START_STOP_BYTE byte
    }
    else if (msg[i] == START_STOP_BYTE && expectStopByte) {  // If receiving START_STOP_BYTE byte
      DEBUG_PRINTLN("Frame terminated, expect receive H-Frame");
      if (!isFrameCorrect(msg, 'H')) {
        DEBUG_PRINTLN("H-frame invalid, not doing anything");
        expectStopByte = false;
        memset(msg, NULL, i + 1);
        i = -1;
        continue;
      }
      else {
        // If handshake, repeat message back to primary
        DEBUG_PRINT("Returning bytes: ");
        DEBUG_WRITE(msg, i + 1);
        DEBUG_PRINTLN("");
        Serial3.write(msg, i + 1);          // RPi Serial
        DEBUG_PRINTLN("Success");
        handshake = true;
        recv_seq = (recv_seq + 1) & 0x7F;   // Keeps the sequence number between 0 - 127 to fit into control_byte

        memset(msg, NULL, i + 1);
        i = -1;
        expectStopByte = false;
      }
    }
    else {  // receiving the other bytes in between
      DEBUG_PRINT("byte is "); DEBUG_WRITE(msg[i]); DEBUG_PRINTLN("");
    }
    delay(10);
  }
}

void setupSensors() {
  for (uint8_t i = 0; i < NUM_GY521; i++) {
    digitalWrite(5, HIGH);
    digitalWrite(6, HIGH);
    digitalWrite(7, HIGH);
    digitalWrite(5 + i, LOW);
    Wire.begin();
    Wire.beginTransmission(MPU);
    Wire.write(0x6B);           // PWR_MGMT_1 register
    Wire.write(0);              // set to zero (wakes up the MPU-6050)
    Wire.endTransmission(true);
    Wire.beginTransmission(MPU);
    Wire.write(0x1C);           // ACCEL_CONFIG register
    Wire.write(8);              // FS_SEL 01: 4g
    Wire.endTransmission(true);
    Wire.beginTransmission(MPU);
    Wire.write(0x1B);           // GYRO_CONFIG register
    Wire.write(16);             // FS_SEL 10: 1000 degrees per second
    Wire.endTransmission(true);
  }
  startTime = millis();
}

uint16_t writeEscaped(char* buf, uint16_t write_start, char* data, uint8_t data_len) {
  // Writes characters in data to buf starting at write_start. Escapes START_STOP_BYTE
  // and ESCAPE_BYTE if they are found. Returns length of data written.
  char byt;
  uint16_t pos = write_start;
  for (uint16_t i = 0; i < data_len; i++) {
    byt = data[i];
    if (byt == START_STOP_BYTE || byt == ESCAPE_BYTE) {
      buf[pos++] = ESCAPE_BYTE;
      buf[pos++] = byt ^ 0x20;
    }
    else {
      buf[pos++] = byt;
    }
  }
  buf[pos] = '\0';
  return pos - write_start;
}

void TaskReadSensors(void *pvParameters) {
  uint16_t buf_len, checksum;
  uint8_t controlBytes_len;    // Calculates control_bytes length
  int AcX[NUM_GY521], AcY[NUM_GY521], AcZ[NUM_GY521];
  int GyX[NUM_GY521], GyY[NUM_GY521], GyZ[NUM_GY521];
  int voltMeasurement, currentMeasurement;
  float voltVal, currentVout, currentVal, powerVal, energyVal = 0;
  char controlBuffer[5];           // a buffer to store the control_bytes when they are escaped.
  char voltStr[8], currentStr[8], powerStr[8], energyStr[8];
  char control_chars[3], check_chars[3];
  unsigned long currentTime, timeDelta;  // startTime defined globallly
  uint32_t qmsg;

  TickType_t prevWakeTime = xTaskGetTickCount();
  for (;;) {
    /*if (freeBuffer <= 0) {  // if no free buffer space left, stop reading new values
      DEBUG_PRINTLN("No more free buffer space!");
      vTaskDelayUntil(&prevWakeTime, xDelay);
      continue;
      }*/
#ifdef DEBUGP
    xSemaphoreTake(xSerialSemaphore, portMAX_DELAY);
#endif

    for (uint8_t i = 0; i < NUM_GY521; i++) {
      digitalWrite(5, HIGH);
      digitalWrite(6, HIGH);
      digitalWrite(7, HIGH);
      digitalWrite(5 + i, LOW);

      Wire.beginTransmission(MPU);
      Wire.write(0x3B);                         // starting with register 0x3B (ACCEL_XOUT_H)
      Wire.endTransmission(false);
      Wire.requestFrom(MPU, 14, true);          // request a total of 14 registers
      AcX[i] = Wire.read() << 8 | Wire.read();  // 0x3B (ACCEL_XOUT_H) & 0x3C (ACCEL_XOUT_L)
      AcY[i] = Wire.read() << 8 | Wire.read();  // 0x3D (ACCEL_YOUT_H) & 0x3E (ACCEL_YOUT_L)
      AcZ[i] = Wire.read() << 8 | Wire.read();  // 0x3F (ACCEL_ZOUT_H) & 0x40 (ACCEL_ZOUT_L)
      Wire.read() << 8 | Wire.read();           // 0x41 (TEMP_OUT_H) & 0x42 (TEMP_OUT_L) Unused - leave because wire.read() goes through this reg
      GyX[i] = Wire.read() << 8 | Wire.read();  // 0x43 (GYRO_XOUT_H) & 0x44 (GYRO_XOUT_L)
      GyY[i] = Wire.read() << 8 | Wire.read();  // 0x45 (GYRO_YOUT_H) & 0x46 (GYRO_YOUT_L)
      GyZ[i] = Wire.read() << 8 | Wire.read();  // 0x47 (GYRO_ZOUT_H) & 0x48 (GYRO_ZOUT_L)

#ifdef DEBUGP
      AcX[i] = AcY[i] = AcZ[i] = GyX[i] = GyY[i] = GyZ[i] = 69800 + 10 * i; // Dummy values, delete in actual
#endif

      Wire.endTransmission(true);
    }

    voltMeasurement = analogRead(VOLT_DIVIDER);
    currentMeasurement = analogRead(VOUT);

    currentTime = millis();
    timeDelta = currentTime - startTime;
    startTime = currentTime;

    voltVal = (float) voltMeasurement * 5 * 2 / 1023.0;                       // Volt, voltage divider halves voltage
    currentVout = (float) currentMeasurement * 5 / 1023.0;                    // INA169 Vout
    currentVal = (currentVout * 1000.0) * 1000.0 / (0.1 * 10000.0);           // mA, Rs = 10 Ohms Rl = 10k Ohms
    powerVal = voltVal * currentVal;                                          // mW
    energyVal = energyVal + ((powerVal / 1000) * ((float) timeDelta / 1000)); // Joules

    dtostrf(voltVal, 0, 2, voltStr);
    dtostrf(currentVal, 0, 0, currentStr);
    dtostrf(powerVal, 0, 0, powerStr);
    dtostrf(energyVal, 0, 1, energyStr);

    // Package into I-frame
    buf_len = controlBytes_len = 0;
    buf_len += sprintf(send_buf[buf_idx] + buf_len,
                       "%c%c",
                       'x', 'x');  // dummy values

    // Recv, send seq
    control_chars[0] = (recv_seq << 1) | 0x1;
    control_chars[1] = send_seq << 1;
    control_chars[2] = '\0';
    // specify data len so write escape doesn't terminate early on send seq 0 = \0
    controlBytes_len += writeEscaped(controlBuffer, controlBytes_len, control_chars, 2);

    buf_len += sprintf(send_buf[buf_idx] + buf_len,
                       "%c%c",
                       control_chars[0], control_chars[1]);

    // Sensor reading - no need to escape since str won't contain ascii 7D={ or 7E=~
    for (uint8_t i = 0; i < NUM_GY521; i++) {
      buf_len += sprintf(send_buf[buf_idx] + buf_len,
                         "%d,%d,%d,%d,%d,%d,",
                         AcX[i], AcY[i], AcZ[i], GyX[i], GyY[i], GyZ[i]);
    }

    // Telemetry - no need to escape since str won't contain ascii 7D={ or 7E=~
    buf_len += sprintf(send_buf[buf_idx] + buf_len,
                       "%s,%s,%s,%s",
                       voltStr, currentStr, powerStr, energyStr);


    // Checksum
    checksum = crc16(&send_buf[buf_idx][2], buf_len - 2);   // Exclude the NULL characters in front
    check_chars[0] = checksum >> 8;
    check_chars[1] = checksum & 0xFF;
    check_chars[2] = '\0';
    // Escape checksum if contains 7D/7E
    buf_len += writeEscaped(send_buf[buf_idx], buf_len, check_chars, 2);
    DEBUG_PRINT("send_buf before including escape bytes is: "); DEBUG_WRITE(send_buf[buf_idx]); DEBUG_PRINTLN("");

    // Write the Escape bytes in front if required
    if (controlBytes_len == 3) {      // 1 control byte escaped
      send_buf[buf_idx][1] = controlBuffer[0];
      send_buf[buf_idx][2] = controlBuffer[1];
      send_buf[buf_idx][3] = controlBuffer[2];
    }
    else if (controlBytes_len == 4) { // 2 control bytes escaped
      send_buf[buf_idx][0] = controlBuffer[0];
      send_buf[buf_idx][1] = controlBuffer[1];
      send_buf[buf_idx][2] = controlBuffer[2];
      send_buf[buf_idx][3] = controlBuffer[3];
    }

    buf_len -= (4 - controlBytes_len);

    qmsg = (unsigned long)(4 - controlBytes_len) << 24 | (unsigned long)buf_idx << 16 | buf_len;   // 1 byte offset, 1 byte index, 2 bytes len
    DEBUG_PRINT("offset is "); DEBUG_PRINTLN(4 - controlBytes_len);

    freeBuffer -= 1;
    buf_idx = (buf_idx + 1) & (BUFFER_SIZE - 1);
    send_seq = (send_seq + 1) & 0x7F;             // Keeps the sequence number between 0 - 127 to fit into control_byte

    // TaskSend handles sending data
    xQueueSend(xSerialSendQueue, &qmsg, portMAX_DELAY);

    // TODO: remove
    // for (uint8_t i = 0; i < NUM_GY521; i++) {
    //   DEBUG_PRINT(i); DEBUG_PRINT(" | ");
    //   DEBUG_PRINT("AcX = ");      DEBUG_PRINT(AcX[i]);
    //   DEBUG_PRINT(" | AcY = ");   DEBUG_PRINT(AcY[i]);
    //   DEBUG_PRINT(" | AcZ = ");   DEBUG_PRINT(AcZ[i]);
    //   DEBUG_PRINT(" | GyX = ");   DEBUG_PRINT(GyX[i]);
    //   DEBUG_PRINT(" | GyY = ");   DEBUG_PRINT(GyY[i]);
    //   DEBUG_PRINT(" | GyZ = "); DEBUG_PRINTLN(GyZ[i]);
    // }

    // DEBUG_PRINT("Voltage(V) = ");        DEBUG_PRINT(voltVal);
    // DEBUG_PRINT(" | Current(mA) = "); DEBUG_PRINT(currentVal);
    // DEBUG_PRINT(" | Power(mW) = ");     DEBUG_PRINT(powerVal);
    // DEBUG_PRINT(" | Energy(J) = ");  DEBUG_PRINTLN(energyVal);
    // DEBUG_PRINT("Checksum: "); DEBUG_PRINTLN(checksum);
#ifdef DEBUGP
    xSemaphoreGive(xSerialSemaphore);
#endif
    vTaskDelayUntil(&prevWakeTime, xDelay);
  }
}


void TaskSend(void *pvParameters) {
  uint8_t byt, buf_read_idx, offset;
  uint16_t len;
  uint32_t qmsg;

  for (;;) {
    xQueueReceive(xSerialSendQueue, &qmsg, portMAX_DELAY);
    xSemaphoreTake(xSerialSemaphore, portMAX_DELAY);

    offset = (qmsg >> 24) & 0xFF;
    buf_read_idx = (qmsg >> 16) & 0xFF;
    len = qmsg & 0xFFFF;

    DEBUG_PRINT("Send Sequence is: "); DEBUG_PRINTLN(send_seq);
    DEBUG_PRINT("send_buf["); DEBUG_PRINT(buf_read_idx); DEBUG_PRINT("] is of length "); DEBUG_PRINTLN(len);

    Serial3.write(START_STOP_BYTE);
    Serial3.write(&send_buf[buf_read_idx][offset], len);
    Serial3.write(START_STOP_BYTE);

    DEBUG_PRINT("Offset is "); DEBUG_PRINTLN(offset);
    DEBUG_PRINT("send_buf after including escape bytes with offset: "); DEBUG_WRITE(&send_buf[buf_read_idx][offset], len); DEBUG_PRINTLN("");
    DEBUG_PRINT("send_buf after including escape bytes w/o offset: "); DEBUG_WRITE(send_buf[buf_read_idx]); DEBUG_PRINTLN("\n");
    lastSent = (lastSent + 1) & (BUFFER_SIZE - 1);
    xSemaphoreGive(xSerialSemaphore);
  }
}

void TaskRecv(void *pvParameters) {
  byte buf[80];               // buf[0] = START, buf[1] = recv_seq, buf[2] = frame, buf[3] = checkNum, buf[4] = checkNum, buf[5] = START_STOP_BYTE
  bool expectStopByte = false;// Signal when a START byte has occured, so that the next such byte is a START_STOP_BYTE byte.
  byte i = -1;                // used to fill buf

  for (;;) {
    xSemaphoreTake(xSerialSemaphore, portMAX_DELAY);
    if (Serial3.available() <= 0) {                // RPi Serial
      delay(10);
      xSemaphoreGive(xSerialSemaphore);
      continue;
    }
    buf[++i] = Serial3.read();                     // RPi Serial
    if (i == 0 && buf[i] != START_STOP_BYTE) {    // if expecting starting byte but receive otherwise
      DEBUG_PRINTLN("Error, Frame does not start with 0x7e");
      i = -1;
      expectStopByte = false;
      memset(buf, NULL, 1);
    }
    else if (buf[i] == START_STOP_BYTE && !expectStopByte) { // START byte received, the next identical byte received will be a START_STOP_BYTE byte
      expectStopByte = true;
    }
    else if (buf[i] == START_STOP_BYTE && expectStopByte) {  // START_STOP_BYTE byte is received, terminate the frame
      DEBUG_PRINTLN("Frame terminated");
      if (buf[2] == final2Bits_HFrame && isFrameCorrect(&buf[0], 'H')) {  // is a H-Frame, verify its correct
        DEBUG_PRINT("Returning bytes: ");
        DEBUG_WRITE(buf, i + 1);
        DEBUG_PRINTLN("");
        Serial3.write(buf, i + 1);         // RPi Serial
        DEBUG_PRINTLN("Success");
      }
      else if ( ( (buf[2] & 0b11) == final2Bits_SFrame) && isFrameCorrect(&buf[0], 'S') ) { // is an S-Frame, verify its correct
        // Check whether need to resend data
        // Trim to only frame[3:2]]. If true, RPi rejected the frame sent by Arduino, must resend
        uint8_t RPiReceive = (buf[1] >> 1) & (BUFFER_SIZE - 1);
        byte SFrameType = (buf[2] >> 2) & 0b11;
        if (SFrameType == SFRAME_REJ) {
          DEBUG_PRINT("RPi has not received all frames, resending frames "); DEBUG_PRINT(RPiReceive); DEBUG_PRINT(" to "); DEBUG_PRINTLN(lastSent);
          if (RPiReceive > lastSent) {
            for (uint8_t i = RPiReceive; i < BUFFER_SIZE; i++) {
              xQueueSend(xSerialSendQueue, &i, portMAX_DELAY);
            }
            for (uint8_t i = 0; i <= lastSent; i++) {
              xQueueSend(xSerialSendQueue, &i, portMAX_DELAY);
            }
          }
          else {
            for (uint8_t i = RPiReceive; i <= lastSent; i++) {
              xQueueSend(xSerialSendQueue, &i, portMAX_DELAY);
            }
          }
        }
        else if ( (SFrameType == SFRAME_RR) || (SFrameType == SFRAME_RNR) ) {
          DEBUG_PRINT("RPi has successully received frames "); DEBUG_PRINT((lastACK + 1) & (BUFFER_SIZE - 1)); DEBUG_PRINT(" to "); DEBUG_PRINTLN((RPiReceive - 1) & (BUFFER_SIZE - 1));
          uint8_t firstACK = (lastACK + 1) & (BUFFER_SIZE - 1); // the first newly ACK frame
          lastACK = (RPiReceive == 0) ? (BUFFER_SIZE - 1) : (RPiReceive - 1);
          freeBuffer += ( (firstACK > lastACK) ? BUFFER_SIZE : 0) + lastACK - firstACK + 1;
          if (firstACK > lastACK) {
            // We do not need to reset the whole buffer as that wastes time. We only reset until at the least STOP byte is cleared even for the shortest possible I-Frame.
            for (uint8_t i = firstACK; i < BUFFER_SIZE; i++) {
              memset(send_buf[i] + MIN_IFRAME_LENGTH, NULL, MAX_IFRAME_LENGTH - MIN_IFRAME_LENGTH);
            }
            for (uint8_t i = 0; i <= lastACK; i++) {
              memset(send_buf[i] + MIN_IFRAME_LENGTH, NULL, MAX_IFRAME_LENGTH - MIN_IFRAME_LENGTH);
            }
          }
          else {
            for (uint8_t i = firstACK; i <= lastACK; i++) {
              memset(send_buf[i] + MIN_IFRAME_LENGTH, NULL, MAX_IFRAME_LENGTH - MIN_IFRAME_LENGTH);
            }
          }
          if (SFrameType == SFRAME_RNR) {
            DEBUG_PRINTLN("Waiting for RPi to finish operations...");
            delay(200);     // RPi might be expanding its CircularBuffer, wait. TO-DO: Is RPi going to signal when its ready?
          }
        }
      }
      //      else if ( ( (buf[1] & 0b1) == 0b1) && ( (buf[2] & 0b1) == 0b0) && isFrameCorrect(&buf[0], 'I') ) { // is an I-Frame, verify its correct
      //        recv_seq = (recv_seq + 1) & 0x7F;     // keeps recv_seq between 0 - 127
      //        char iFrameMsg[50];
      //        byte i = 0;
      //        iFrameMsg[i++] = buf[3];
      //        for (byte j = 4; buf[j + 2] != START_STOP_BYTE; j++) { // since checksum is 2 bytes long, followed by START_STOP_BYTE byte, stop when hit 1st checksum byte
      //          iFrameMsg[i++] = buf[j];
      //        }
      //        iFrameMsg[i] = '\0';  // terminate the string properly
      //        DEBUG_PRINT("The I-Frame Message is: " ); DEBUG_PRINTLN(iFrameMsg);
      //        // if(iFrameMsg.equals("send me data")) {   // Intepret the message sent in I-Frame
      //        // Send S-Frame back to RPi
      //        DEBUG_PRINTLN("I-Frame received! Sending S-Frame");
      //        DEBUG_WRITE(START_STOP_BYTE);
      //        byte msg[2];
      //        msg[0] = buf[1];
      //        msg[1] = 0x01;
      //        DEBUG_WRITE(msg[0]);
      //        DEBUG_WRITE(msg[1]);
      //        int checkNum = crc16(&msg[0], 2);
      //        DEBUG_WRITE(checkNum);
      //        DEBUG_WRITE(START_STOP_BYTE);
      //      }
      memset(buf, NULL, i + 1);
      i = -1;
      expectStopByte = false;
    }
    else {
      // Do nothing
    }
    xSemaphoreGive(xSerialSemaphore);
  }
}

bool isFrameCorrect(byte* buf, char type) {
  // Calculates the length of buf. Also START byte check.
  byte len;
  if (buf[0] != START_STOP_BYTE)  return false; // does not start with START byte, fail
  else                            len = 1;      // start with the first data byte

  while (buf[len] != START_STOP_BYTE && len < 100) { // Arbitary number 100 set to prevent infinite loop in case frame does not terminate properly
    len++;
  }
  len++;                                  // include START_STOP_BYTE byte as part of len

  // Checks control_byte2
  byte check;
  bool isFrameCorrect;
  if (type == 'H')      check = final2Bits_HFrame;
  else if (type == 'S') check = final2Bits_SFrame;

  if (type == 'I')  isFrameCorrect = !(buf[2] & 0b1);             // checks whether bit[0] == 0;
  else              isFrameCorrect = ( (buf[2] & 0b11) == check);
  //

  int checkNum = buf[len - 3] << 8 | buf[len - 2];
  //DEBUG_PRINT("Calc checksum is "); DEBUG_PRINT(crc16(&buf[1], len - 4)); DEBUG_PRINT(" vs input checksum is "); DEBUG_PRINTLN(checkNum);
  return (buf[1] & 0b1) && isFrameCorrect && crc16(&buf[1], len - 4) == checkNum && buf[len - 1] == START_STOP_BYTE;
}

uint16_t crc16(char* buf, int len) {
  char str1[5], str2[5];
  sprintf(str1, "%02X", buf[0]);
  sprintf(str2, "%02X", buf[len - 1]);
  DEBUG_PRINT("first byte: ");  DEBUG_PRINT(str1);
  DEBUG_PRINT(", last byte: "); DEBUG_PRINTLN(str2);

  uint16_t remainder = 0x0000;
  uint16_t poly = 0x1021;
  for (int byte = 0; byte < len; ++byte) {
    remainder ^= (buf[byte] << 8);
    for (uint8_t bit = 8; bit > 0; --bit) {
      if (remainder & 0x8000) remainder = (remainder << 1) ^ poly;
      else                    remainder = (remainder << 1);
    }
  }
  return remainder;
}

void loop() {
  // Empty. Things are done in Tasks.
}
