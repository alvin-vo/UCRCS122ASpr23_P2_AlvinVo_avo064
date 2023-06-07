#include <LiquidCrystal.h>
#include <Servo.h>
#include <SPI.h>
#include <MFRC522.h>
#include <NewPing.h>

// RFID Variables
#define RFID_CARD "BF E2 42 03"
#define RFID_TAG "50 8C DB 1E"
#define SS_PIN 10
#define RST_PIN 9
String PASSWORD = "letmeinpleasethx"; // Located on block 1
String RFID_name; // Located on block 4
bool RFID_activated;
int RFID_check;

// Ultrasonic Variables
#define ECHO_PIN 7
#define TRIG_PIN 6
#define MAX_DISTANCE 200 // in cm
bool SONAR_detected;

// Servo Variables
#define SERV_PIN 8
int SERV_angle;

// LCD Variables
#define RS 3
#define EN 2
#define D4 A0
#define D5 A1
#define D6 A2
#define D7 A3

// Create instances
NewPing sonar(TRIG_PIN, ECHO_PIN, MAX_DISTANCE);
MFRC522 mfrc522(SS_PIN, RST_PIN);
Servo servo;
LiquidCrystal lcd(RS, EN, D4, D5, D6, D7);

// Task Scheduler
typedef struct task {
  int state;
  unsigned long period;
  unsigned long elapsedTime;
  int (*TickFct)(int);
} task;

const unsigned short tasksNum = 4;
task tasks[tasksNum];

enum RFID_States { RFID_INIT, RFID_IDLE, RFID_READ, RFID_INVALID, RFID_CONFIRM1, RFID_CONFIRM2, RFID_VALID };
int RFID_Tick(int state) {

  static String content = "";
  static byte count = 0;
  static byte buffer1[18], buffer2[18];
  byte block1 = 1, len1 = 18;
  byte block2 = 4, len2 = 18;
  static MFRC522::MIFARE_Key key;
  static MFRC522::StatusCode status1, status2;
  static String password = "";

  switch (state) {  // TRANSITIONS
    case RFID_INIT:
      Serial.println("**Place RFID next to reader**");
      for (byte i = 0; i < 6; i++) 
        key.keyByte[i] = 0xFF;
      RFID_activated = 0;
      RFID_check = 0;
      state = RFID_IDLE;
      break;
    case RFID_IDLE:
      mfrc522.PICC_HaltA();
      mfrc522.PCD_StopCrypto1();
      state = (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) ? RFID_READ : RFID_IDLE;
      break;
    case RFID_READ:
      status1 = mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, block1, &key, &(mfrc522.uid));
      if (status1 != MFRC522::STATUS_OK) {
        state = RFID_INVALID;
        break;
      }
      status1 = mfrc522.MIFARE_Read(block1, buffer1, &len1);
      if (status1 != MFRC522::STATUS_OK) {
        state = RFID_INVALID;
        break;
      }

      status2 = mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, block2, &key, &(mfrc522.uid));
      if (status2 != MFRC522::STATUS_OK) {
        state = RFID_INVALID;
        break;
      }
      status2 = mfrc522.MIFARE_Read(block2, buffer2, &len2);
      if (status2 != MFRC522::STATUS_OK) {
        state = RFID_INVALID;
        break;
      }
      state = RFID_CONFIRM1;
      break;
    case RFID_INVALID:
      state = (count++ < 10) ? RFID_INVALID : RFID_IDLE;
      break;
    case RFID_CONFIRM1:
      state = RFID_CONFIRM2;
      break;
    case RFID_CONFIRM2:
      state = password == PASSWORD ? RFID_VALID : RFID_INVALID;
      break;
    case RFID_VALID: 
      state = (count++ < 10) ? RFID_VALID : RFID_IDLE;
      break;
  }

  switch (state) {  // ACTIONS
    case RFID_INIT:
      break;
    case RFID_IDLE:
      count = 0;
      RFID_activated = 0;
      password = "";
      RFID_name = "";
      RFID_check = 0;
      break;
    case RFID_READ:
      Serial.print("UID tag :");
      for (byte i = 0; i < mfrc522.uid.size; i++) {
        Serial.print(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");
        Serial.print(mfrc522.uid.uidByte[i], HEX);
        content.concat(String(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " "));
        content.concat(String(mfrc522.uid.uidByte[i], HEX));
      }
      Serial.println();
      break;
    case RFID_INVALID:
      Serial.println("Inauthorized access!");
      content = "";
      RFID_check = 1;
      break;
    case RFID_CONFIRM1: // grabs password in buffer1
      for (uint8_t i = 0; i < 16; i++) {
        Serial.write(buffer1[i]);
        password += (char)buffer1[i];
      }
      Serial.println("||" + PASSWORD);
      break;
    case RFID_CONFIRM2: // grabs name in buffer2
      for (uint8_t i = 0; i < 16; i++) {
        Serial.write(buffer2[i]);
        RFID_name += (char)buffer2[i];
      }
      break;
    case RFID_VALID:
      Serial.println("Authorized access!");
      RFID_activated = 1;
      content = "";
      RFID_check = 2;
      break;
  }
  return state;
}

enum SONAR_States { SONAR_INIT, SONAR_OFF, SONAR_ON };
int SONAR_Tick(int state) {
  switch (state) { // TRANSITIONS
    case SONAR_INIT:
      SONAR_detected = 0;
      state = SONAR_OFF;
      break;
    case SONAR_OFF:
      state = sonar.ping_cm() != 0 ? SONAR_ON : SONAR_OFF;
      break;
    case SONAR_ON:
      state = sonar.ping_cm() != 0 ? SONAR_ON : SONAR_OFF;
      break;
  }
  switch (state) { // ACTIONS
    case SONAR_INIT:
      break;
    case SONAR_OFF:
      SONAR_detected = 0;
      break;
    case SONAR_ON:
      SONAR_detected = 1;
      break;
  }
  return state;
}

enum SERV_States { SERV_INIT, SERV_LOCK, SERV_UNLOCK };
int SERV_Tick(int state) {
  bool access = RFID_activated && SONAR_detected;
  switch (state) { // TRANSITIONS
    case SERV_INIT:
      SERV_angle = 45;
      servo.write(SERV_angle);
      state = SERV_LOCK;
      break;
    case SERV_LOCK:
      state = access ? SERV_UNLOCK : SERV_LOCK;
      break;
    case SERV_UNLOCK:
      state = access ? SERV_UNLOCK : SERV_LOCK;
      break;
  }
  switch (state) { // ACTIONS
    case SERV_INIT:
      break;
    case SERV_LOCK:
      if (SERV_angle < 45){
        servo.write(SERV_angle++);
      }
      break;
    case SERV_UNLOCK:
      if (SERV_angle > 0) {
        servo.write(SERV_angle--);
      }
      break;
  }
  return state;
}

enum LCD_States { LCD_INIT, LCD_CLEAR, LCD_WELCOME, LCD_NOTWELCOME};
int LCD_Tick(int state) {
  switch (state) { // TRANSITIONS
    case LCD_INIT:
      lcd.begin(12, 2);
      state = LCD_CLEAR;
      break;
    case LCD_CLEAR:
      if (RFID_check == 0) {
        state = LCD_CLEAR;
      } else if (RFID_check == 1 && SONAR_detected) {
        state = LCD_NOTWELCOME;
      } else if (RFID_check == 2 && SONAR_detected) {
        state = LCD_WELCOME;
      }
      break;
    case LCD_WELCOME:
      if (RFID_check == 0) {
        state = LCD_CLEAR;
      } else if (RFID_check == 1) {
        state = LCD_NOTWELCOME;
      } else if (RFID_check == 2) {
        state = LCD_WELCOME;
      }
      break;
    case LCD_NOTWELCOME:
      if (RFID_check == 0) {
        state = LCD_CLEAR;
      } else if (RFID_check == 1) {
        state = LCD_NOTWELCOME;
      } else if (RFID_check == 2) {
        state = LCD_WELCOME;
      }
      break;
  }
  switch (state) { // ACTIONS
    case LCD_INIT:
      break;
    case LCD_CLEAR:
      lcd.clear();
      break;
    case LCD_WELCOME:
      lcd.setCursor(0, 0);
      lcd.print("Welcome ");
      lcd.setCursor(0, 1);
      lcd.print(RFID_name);
      break;
    case LCD_NOTWELCOME:
      lcd.setCursor(0, 0);
      lcd.print("No access");
      lcd.setCursor(0, 1);
      lcd.print(RFID_name);
      break;
  }
  return state;
}

void setup() {
  Serial.begin(9600);   // Initiate a serial communication
  SPI.begin();      // Initiate  SPI bus
  mfrc522.PCD_Init();   // Initiate MFRC522
  servo.attach(SERV_PIN);

  // Task Scheduler
  unsigned char i = 0;
  tasks[i].state = RFID_INIT;
  tasks[i].period = 500;
  tasks[i].elapsedTime = 0;
  tasks[i].TickFct = &RFID_Tick;
  i++;
  tasks[i].state = SONAR_INIT;
  tasks[i].period = 1000;
  tasks[i].elapsedTime = 0;
  tasks[i].TickFct = &SONAR_Tick;
  i++;
  tasks[i].state = SERV_INIT;
  tasks[i].period = 25;
  tasks[i].elapsedTime = 0;
  tasks[i].TickFct = &SERV_Tick;
  i++;
  tasks[i].state = LCD_INIT;
  tasks[i].period = 250;
  tasks[i].elapsedTime = 0;
  tasks[i].TickFct = &LCD_Tick;
}

void loop() {
  for (unsigned char i = 0; i < tasksNum; ++i) {
    if ((millis() - tasks[i].elapsedTime) >= tasks[i].period) {
      tasks[i].state = tasks[i].TickFct(tasks[i].state);
      tasks[i].elapsedTime = millis();
    }
  }
}