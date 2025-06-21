/*
  ESP32 - Kirim Data Sampah ke Firebase Realtime Database

  Program ini digunakan untuk menerima data klasifikasi dari Arduino Nano melalui komunikasi 
  serial, membaca berat dan volume sampah dari sensor load cell dan ultrasonik, lalu mengirim
  semua data tersebut ke Firebase Realtime Database. 

  FIREBASE_HOST merupakan URL database yang telah dibuat
  FIREBASE_AUTH merupakan Secret Key (Database secrets) atau API Key 
*/

#include <HX711.h>
#include "ESP32Servo.h"
#include <NewPing.h>
#include "U8g2lib.h"
#include "DFRobotDFPlayerMini.h"
#include "Arduino.h"
#include <SoftwareSerial.h>
#include <WiFi.h>
#include <FirebaseESP32.h>
#include <addons/RTDBHelper.h>

const char* ssid = "SSID_WIFI";
const char* password = "PASSWORD_WIFI";

#define FIREBASE_HOST "URL_DATABASE"
#define FIREBASE_AUTH "SECRET_KEY"

FirebaseData fbdo;
FirebaseConfig config;
FirebaseAuth auth;
FirebaseJson json;

unsigned long sendDataPrevMillis = 0;
String bin_id = "W001";   // untuk kode waste
String dynamicPath = "/bins/" + bin_id;   // Path dinamis dan path utama berdasarkan bin_id
bool isValidBinID = false;

#define LOADCELL1_DOUT_PIN 32
#define LOADCELL1_SCK_PIN 33
#define LOADCELL2_DOUT_PIN 26
#define LOADCELL2_SCK_PIN 27
#define LOADCELL3_DOUT_PIN 2
#define LOADCELL3_SCK_PIN 4
#define calibration_factor -205.00  // Faktor kalibrasi untuk timbangan

HX711 scale1;
HX711 scale2;
HX711 scale3;

int GRAM1, GRAM2, GRAM3;  // variabel weight

int df_TX = 17;  
int df_RX = 16;  

NewPing sonar1(19, 36, 100);  // (triggerPin, echoPin, maxDistance)
NewPing sonar2(23, 39, 100);
NewPing sonar3(18, 5, 100);

int jarak1, jarak2, jarak3;
int persenjarak1, persenjarak2, persenjarak3;  // variabel volume

DFRobotDFPlayerMini player;
SoftwareSerial softwareSerial(df_RX, df_TX);

Servo servoOrganik;
Servo servoAnorganik;
Servo servoLogam;

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, SCL, SDA, U8X8_PIN_NONE);

void setup() {
  Serial.begin(115200);  
  softwareSerial.begin(9600);

  WiFi.mode(WIFI_OFF);
  delay(1000);
  WiFi.mode(WIFI_STA);

  WiFi.begin(ssid, password);
  Serial.println("Connecting to WiFi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.print("RSSI (kekuatan sinyal): ");
  Serial.println(WiFi.RSSI());
  
  // Inisialisasi Firebase
  config.host = FIREBASE_HOST;
  config.signer.tokens.legacy_token = FIREBASE_AUTH;
  Firebase.begin(&config, &auth);
  fbdo.setBSSLBufferSize(4096, 1024);
  Firebase.reconnectWiFi(true);

  scale1.begin(LOADCELL1_DOUT_PIN, LOADCELL1_SCK_PIN);
  scale2.begin(LOADCELL2_DOUT_PIN, LOADCELL2_SCK_PIN);
  scale3.begin(LOADCELL3_DOUT_PIN, LOADCELL3_SCK_PIN);

  // Atur faktor kalibrasi dan tare untuk setiap load cell
  scale1.set_scale(calibration_factor);
  scale1.tare();  // Mengatur titik nol pada timbangan 

  scale2.set_scale(calibration_factor);
  scale2.tare();  

  scale3.set_scale(calibration_factor);
  scale3.tare();  

  if (player.begin(softwareSerial)) {
    Serial.println("ok");
    player.volume(100);
  } else {
    Serial.println("cek kabel");
  }

  servoOrganik.attach(13);    // Ganti dengan pin yang sesuai
  servoAnorganik.attach(12);  
  servoLogam.attach(14);      
  u8g2.begin();
  u8g2.enableUTF8Print();
  u8g2.setFlipMode(1);
  loading();
  isValidBinID = validateBinID(bin_id);
}

void loop() {
  String receivedData = Serial.readStringUntil('\n');  // Membaca data hingga newline
  Serial.print(receivedData);
  Serial.println();

  // Mengecek hasil klasifikasi dan menggerakkan servo yang sesuai
  if (receivedData.indexOf("ANORGANIK") >= 0) {
    player.play(1);
    mata_ramah();
    delay(1000);
    servoAnorganik.write(130);
    mata_bawah();
    volume_sampah();
    delay(1000);
    berat_sampah();
    delay(1000);
    servoAnorganik.write(0);
    mata_tutup();
    loading();
    if (Firebase.ready() && (millis() - sendDataPrevMillis > 15000)) {
      sendDataPrevMillis = millis();
      if (isValidBinID) {
        url_anorganik();
      } else {
        Serial.println("Invalid bin_id, skipping data send.");
      }
    }

  } else if (receivedData.indexOf("ORGANIK") >= 0) {
    player.play(2);
    mata_ramah();
    delay(1000);
    servoOrganik.write(100);
    mata_kanan();
    volume_sampah();
    delay(1000);
    berat_sampah();
    delay(1000);
    servoOrganik.write(0);
    mata_tutup();
    loading();
    if (Firebase.ready() && (millis() - sendDataPrevMillis > 15000)) {
      sendDataPrevMillis = millis();
      if (isValidBinID) {
        url_organik();
      } else {
        Serial.println("Invalid bin_id, skipping data send.");
      }
    }

  } else if (receivedData.indexOf("LOGAM") >= 0) {
    player.play(3);
    mata_ramah();
    delay(1000);
    servoLogam.write(100);
    mata_kiri();
    volume_sampah();
    delay(1000);
    berat_sampah();
    delay(1000);
    servoLogam.write(0);
    mata_tutup();
    loading();
    if (Firebase.ready() && (millis() - sendDataPrevMillis > 15000)) {
      sendDataPrevMillis = millis();
      if (isValidBinID) {
        url_logam();
      } else {
        Serial.println("Invalid bin_id, skipping data send.");
      }
    }
  }
}

bool validateBinID(const String &binID) {
  if (Firebase.get(fbdo, dynamicPath)) {
    Serial.println("BinID valid: " + bin_id);
    return true;
  } else {
    Serial.println("Invalid BinID: " + bin_id + ", Error: " + fbdo.errorReason());
    return false;
  }
}

void kirim_json(const String &path, FirebaseJson &json, const String &successMsg) {
  const int maxRetries = 2;   // Jumlah maksimal percobaan
  int attempt = 0;
  bool success = false;

  // Mencoba mengirim data beberapa kali
  while (attempt < maxRetries && !success) {
    if (Firebase.updateNode(fbdo, path, json)) {
      success = true;
      Serial.println(successMsg);
    } else {
      Serial.print("Gagal mengirim data ke ");
      Serial.print(path);
      Serial.print(": ");
      Serial.println(fbdo.errorReason().c_str());
      attempt++;
      delay(500);
    }
  }
  if (!success) {
    Serial.println("Pengiriman data gagal setelah beberapa percobaan.");
  }
  json.clear();
}

void url_organik() {
  json.set("/binID", bin_id);
  json.set("/organik/volume", persenjarak1);
  json.set("/organik/weight", GRAM1);
  kirim_json(dynamicPath, json, "Data organik berhasil dikirim!");
}

void url_anorganik() {
  json.set("/binID", bin_id);
  json.set("/anorganik/volume", persenjarak2);
  json.set("/anorganik/weight", GRAM2);
  kirim_json(dynamicPath, json, "Data anorganik berhasil dikirim!");
}

void url_logam() {
  json.set("/binID", bin_id);
  json.set("/logam/volume", persenjarak3);
  json.set("/logam/weight", GRAM3);
  kirim_json(dynamicPath, json, "Data logam berhasil dikirim!");
}

// (membuka mata robot)
void loading() {  
  u8g2.clearBuffer();
  u8g2.drawBox(2, 31, 2, 2);
  u8g2.sendBuffer();
  delay(10);
  u8g2.drawBox(4, 31, 2, 2);
  u8g2.sendBuffer();
  delay(5);
  u8g2.drawBox(6, 31, 2, 2);
  u8g2.sendBuffer();
  delay(5);
  u8g2.drawBox(8, 31, 2, 2);
  u8g2.sendBuffer();
  delay(5);
  u8g2.drawBox(10, 31, 2, 2);
  u8g2.sendBuffer();
  delay(5);
  u8g2.drawBox(12, 31, 2, 2);
  u8g2.sendBuffer();
  delay(5);
  u8g2.drawBox(14, 31, 2, 2);
  u8g2.sendBuffer();
  delay(5);
  u8g2.drawBox(16, 31, 2, 2);
  u8g2.sendBuffer();
  delay(5);
  u8g2.drawBox(18, 31, 2, 2);
  u8g2.sendBuffer();
  delay(5);
  u8g2.drawBox(20, 31, 2, 2);
  u8g2.sendBuffer();
  delay(5);
  u8g2.drawBox(22, 31, 2, 2);
  u8g2.sendBuffer();
  delay(5);
  u8g2.drawBox(24, 31, 2, 2);
  u8g2.sendBuffer();
  delay(5);
  u8g2.drawBox(26, 31, 2, 2);
  u8g2.sendBuffer();
  delay(5);
  u8g2.drawBox(28, 31, 2, 2);
  u8g2.sendBuffer();
  delay(5);
  u8g2.drawBox(30, 31, 2, 2);
  u8g2.sendBuffer();
  delay(5);
  u8g2.drawBox(32, 31, 2, 2);
  u8g2.sendBuffer();
  delay(5);
  u8g2.drawBox(34, 31, 2, 2);
  u8g2.sendBuffer();
  delay(5);
  u8g2.drawBox(36, 31, 2, 2);
  u8g2.sendBuffer();
  delay(5);
  u8g2.drawBox(38, 31, 2, 2);
  u8g2.sendBuffer();
  delay(5);
  u8g2.drawBox(40, 31, 2, 2);
  u8g2.sendBuffer();
  delay(5);
  u8g2.drawBox(42, 31, 2, 2);
  u8g2.sendBuffer();
  delay(5);
  u8g2.drawBox(44, 31, 2, 2);
  u8g2.sendBuffer();
  delay(5);
  u8g2.drawBox(46, 31, 2, 2);
  u8g2.sendBuffer();
  delay(5);
  u8g2.drawBox(48, 31, 2, 2);
  u8g2.sendBuffer();
  delay(5);
  u8g2.drawBox(50, 31, 2, 2);
  u8g2.sendBuffer();
  delay(5);
  u8g2.drawBox(52, 31, 2, 2);
  u8g2.sendBuffer();
  delay(5);
  u8g2.drawBox(54, 31, 2, 2);
  u8g2.sendBuffer();
  delay(5);
  u8g2.drawBox(56, 31, 2, 2);
  u8g2.sendBuffer();
  delay(5);
  u8g2.drawBox(58, 31, 2, 2);
  u8g2.sendBuffer();
  delay(5);
  u8g2.drawBox(60, 31, 2, 2);
  u8g2.sendBuffer();
  delay(5);
  u8g2.drawBox(62, 31, 2, 2);
  u8g2.sendBuffer();
  delay(5);
  u8g2.drawBox(64, 31, 2, 2);
  u8g2.sendBuffer();
  delay(5);
  u8g2.drawBox(66, 31, 2, 2);
  u8g2.sendBuffer();
  delay(5);
  u8g2.drawBox(68, 31, 2, 2);
  u8g2.sendBuffer();
  delay(5);
  u8g2.drawBox(70, 31, 2, 2);
  u8g2.sendBuffer();
  delay(5);
  u8g2.drawBox(72, 31, 2, 2);
  u8g2.sendBuffer();
  delay(5);
  u8g2.drawBox(74, 31, 2, 2);
  u8g2.sendBuffer();
  delay(5);
  u8g2.drawBox(76, 31, 2, 2);
  u8g2.sendBuffer();
  delay(5);
  u8g2.drawBox(78, 31, 2, 2);
  u8g2.sendBuffer();
  delay(5);
  u8g2.drawBox(80, 31, 2, 2);
  u8g2.sendBuffer();
  delay(5);
  u8g2.drawBox(82, 31, 2, 2);
  u8g2.sendBuffer();
  delay(5);
  u8g2.drawBox(84, 31, 2, 2);
  u8g2.sendBuffer();
  delay(5);
  u8g2.drawBox(86, 31, 2, 2);
  u8g2.sendBuffer();
  delay(5);
  u8g2.drawBox(88, 31, 2, 2);
  u8g2.sendBuffer();
  delay(5);
  u8g2.drawBox(90, 31, 2, 2);
  u8g2.sendBuffer();
  delay(5);
  u8g2.drawBox(92, 31, 2, 2);
  u8g2.sendBuffer();
  delay(5);
  u8g2.drawBox(94, 31, 2, 2);
  u8g2.sendBuffer();
  delay(5);
  u8g2.drawBox(96, 31, 2, 2);
  u8g2.sendBuffer();
  delay(5);
  u8g2.drawBox(98, 31, 2, 2);
  u8g2.sendBuffer();
  delay(5);
  u8g2.drawBox(100, 31, 2, 2);
  u8g2.sendBuffer();
  delay(5);
  u8g2.drawBox(102, 31, 2, 2);
  u8g2.sendBuffer();
  delay(5);
  u8g2.drawBox(104, 31, 2, 2);
  u8g2.drawRBox(60, 28, 10, 8, 4);
  u8g2.sendBuffer();
  delay(5);
  u8g2.drawBox(106, 31, 2, 2);
  u8g2.drawRBox(55, 28, 20, 8, 4);
  u8g2.sendBuffer();
  delay(5);
  u8g2.drawBox(108, 31, 2, 2);
  u8g2.drawRBox(50, 28, 30, 8, 4);
  u8g2.sendBuffer();
  delay(5);
  u8g2.drawBox(110, 31, 2, 2);
  u8g2.drawRBox(45, 28, 40, 8, 4);
  u8g2.sendBuffer();
  delay(5);
  u8g2.drawBox(112, 31, 2, 2);
  u8g2.drawRBox(40, 28, 50, 8, 4);
  u8g2.sendBuffer();
  delay(5);
  u8g2.drawBox(114, 31, 2, 2);
  u8g2.drawRBox(35, 28, 60, 8, 4);
  u8g2.sendBuffer();
  delay(5);
  u8g2.drawBox(116, 31, 2, 2);
  u8g2.drawRBox(30, 28, 70, 8, 4);
  u8g2.sendBuffer();
  delay(5);
  u8g2.drawBox(118, 31, 2, 2);
  u8g2.drawRBox(25, 28, 80, 8, 4);
  u8g2.sendBuffer();
  delay(5);
  u8g2.drawBox(120, 31, 2, 2);
  u8g2.sendBuffer();
  delay(5);
  u8g2.drawBox(122, 31, 2, 2);
  u8g2.sendBuffer();
  delay(5);
  u8g2.drawBox(124, 31, 2, 2);
  u8g2.drawRBox(25, 19, 80, 25, 4);
  u8g2.sendBuffer();
  delay(5);
  u8g2.drawBox(126, 31, 2, 2);
  u8g2.drawRBox(25, 2, 80, 60, 8);
  u8g2.sendBuffer();
  delay(5);
  u8g2.clearBuffer();
  u8g2.drawRBox(25, 2, 80, 60, 8);
  u8g2.sendBuffer();
}

void mata_tutup() {
  u8g2.clearBuffer();
  u8g2.drawRBox(25, 2, 80, 60, 8);
  u8g2.sendBuffer();
  delay(50);
  u8g2.clearBuffer();
  u8g2.drawRBox(25, 19, 80, 25, 4);
  u8g2.sendBuffer();
  delay(50);
  u8g2.clearBuffer();
  u8g2.drawRBox(25, 28, 80, 8, 4);
  u8g2.sendBuffer();
  delay(50);
  u8g2.clearBuffer();
  u8g2.drawBox(25, 31, 80, 2);
  u8g2.sendBuffer();
  delay(50);
  u8g2.clearBuffer();
  u8g2.drawBox(128, 31, 0, 0);
  u8g2.sendBuffer();
  delay(50);
}

// (mata bergerak keatas untuk menyapa)
void mata_ramah() {
  u8g2.clearBuffer();
  u8g2.drawRBox(25, 0, 80, 60, 8);
  u8g2.sendBuffer();
  delay(5);
  u8g2.clearBuffer();
  u8g2.drawRBox(25, 0, 80, 50, 8);
  u8g2.sendBuffer();
  delay(5);
  u8g2.clearBuffer();
  u8g2.drawRBox(25, 0, 80, 40, 8);
  u8g2.sendBuffer();
  delay(5);
  u8g2.drawBox(20, 37, 90, 3);
  u8g2.sendBuffer();
  delay(5);
  u8g2.drawBox(19, 38, 92, 1);
  u8g2.sendBuffer();
  delay(5);
  u8g2.drawBox(18, 39, 94, 1);
  u8g2.sendBuffer();
  delay(5);
  u8g2.drawBox(17, 40, 45, 1);
  u8g2.drawBox(16, 41, 44, 1);
  u8g2.sendBuffer();
  delay(5);
  u8g2.drawBox(68, 40, 45, 1);
  u8g2.drawBox(70, 41, 44, 1);
  u8g2.sendBuffer();
}

// (mata bergerak untuk menunjukan bagian kanan pada alat)
void mata_kanan() {
  u8g2.clearBuffer();
  u8g2.drawRBox(25, 2, 80, 60, 8);
  u8g2.sendBuffer();
  delay(20);
  u8g2.clearBuffer();
  u8g2.drawRBox(25, 2, 65, 60, 8);
  u8g2.sendBuffer();
  delay(20);
  u8g2.clearBuffer();
  u8g2.drawRBox(25, 2, 50, 60, 8);
  u8g2.sendBuffer();
  delay(20);
  u8g2.clearBuffer();
  u8g2.drawRBox(10, 3, 50, 60, 8);
  u8g2.sendBuffer();
  delay(20);
  u8g2.clearBuffer();
  u8g2.drawRBox(1, 4, 50, 60, 8);
  u8g2.sendBuffer();
  delay(3000);  // (lama berhenti mata ke kanan)
  u8g2.clearBuffer();
  u8g2.drawRBox(10, 3, 50, 60, 8);
  u8g2.sendBuffer();
  delay(10);
  u8g2.clearBuffer();
  u8g2.drawRBox(25, 2, 50, 60, 8);
  u8g2.sendBuffer();
  delay(10);
  u8g2.clearBuffer();
  u8g2.drawRBox(25, 2, 65, 60, 8);
  u8g2.sendBuffer();
  delay(10);
  u8g2.clearBuffer();
  u8g2.drawRBox(25, 2, 80, 60, 8);
  u8g2.sendBuffer();
  delay(1000);
}

// (mata bergerak untuk menunjukan bagian kiri pada alat)
void mata_kiri() {
  u8g2.clearBuffer();
  u8g2.drawRBox(25, 2, 80, 60, 8);
  u8g2.sendBuffer();
  delay(20);
  u8g2.clearBuffer();
  u8g2.drawRBox(40, 2, 65, 60, 8);
  u8g2.sendBuffer();
  delay(20);
  u8g2.clearBuffer();
  u8g2.drawRBox(55, 2, 50, 60, 8);
  u8g2.sendBuffer();
  delay(20);
  u8g2.clearBuffer();
  u8g2.drawRBox(66, 3, 50, 60, 8);
  u8g2.sendBuffer();
  delay(20);
  u8g2.clearBuffer();
  u8g2.drawRBox(78, 4, 50, 60, 8);
  u8g2.sendBuffer();
  delay(3000);  // (lama berhenti mata ke kiri)
  u8g2.clearBuffer();
  u8g2.drawRBox(66, 3, 50, 60, 8);
  u8g2.sendBuffer();
  delay(10);
  u8g2.clearBuffer();
  u8g2.drawRBox(55, 2, 50, 60, 8);
  u8g2.sendBuffer();
  delay(10);
  u8g2.clearBuffer();
  u8g2.drawRBox(40, 2, 65, 60, 8);
  u8g2.sendBuffer();
  delay(10);
  u8g2.clearBuffer();
  u8g2.drawRBox(25, 2, 80, 60, 8);
  u8g2.sendBuffer();
  delay(1000);
}

void mata_bawah() {
  u8g2.clearBuffer();
  u8g2.drawRBox(25, 2, 80, 60, 8);
  u8g2.sendBuffer();
  delay(10);
  u8g2.clearBuffer();
  u8g2.drawRBox(25, 13, 80, 50, 8);
  u8g2.sendBuffer();
  delay(10);
  u8g2.clearBuffer();
  u8g2.drawRBox(25, 24, 80, 40, 8);
  u8g2.sendBuffer();
  delay(3000);
  u8g2.clearBuffer();
  u8g2.drawRBox(25, 13, 80, 50, 8);
  u8g2.sendBuffer();
  delay(10);
  u8g2.clearBuffer();
  u8g2.drawRBox(25, 2, 80, 60, 8);
  u8g2.sendBuffer();
  delay(1000);
}

void volume_sampah() {
  jarak1 = sonar1.ping_cm();
  jarak2 = sonar2.ping_cm();
  jarak3 = sonar3.ping_cm();
  persenjarak1 = map(jarak1, 45, 0, 20, 100);
  persenjarak2 = map(jarak2, 45, 0, 20, 100);
  persenjarak3 = map(jarak3, 30, 0, 0, 100);
  if (jarak1 > 45) {
    persenjarak1 = 0;
  }
  if (jarak2 > 45) {
    persenjarak2 = 0;
  }
  if (jarak3 > 30) {
    persenjarak3 = 0;
  }
  u8g2.clearBuffer();
  u8g2.drawRBox(25, 2, 80, 60, 8);
  u8g2.sendBuffer();
  delay(50);
  u8g2.drawRBox(15, 2, 104, 60, 8);
  u8g2.sendBuffer();
  delay(50);
  u8g2.drawRBox(5, 2, 114, 60, 8);
  u8g2.sendBuffer();
  delay(50);

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr);
  u8g2.setCursor(8, 17);
  u8g2.print("VOLUME SAMPAH");
  u8g2.setCursor(8, 32);
  u8g2.print("ORGANIK       :" + String(persenjarak1) + String("%"));
  u8g2.setCursor(9, 42);
  u8g2.print("ANORGANIK :" + String(persenjarak2) + String("%"));
  u8g2.setCursor(9, 52);
  u8g2.print("LOGAM           :" + String(persenjarak3) + String("%"));
  u8g2.drawFrame(5, 21, 114, 34);
  u8g2.sendBuffer();
}

void berat_sampah() {
  scale1.set_scale(calibration_factor);
  scale2.set_scale(calibration_factor);
  scale3.set_scale(calibration_factor);

  // Membaca berat dalam gram dan menyimpannya dalam variabel masing-masing
  GRAM1 = scale1.get_units(), 10;  // Membaca 10 sampel dan mengembalikan rata-rata
  GRAM2 = scale2.get_units(), 10;
  GRAM3 = scale3.get_units(), 10;

  if (GRAM1 < 0) {
    GRAM1 = 0;
  }
  if (GRAM2 < 0) {
    GRAM2 = 0;
  }
  if (GRAM3 < 0) {
    GRAM3 = 0;
  }

  // Tampilkan nilai berat pada Serial Monitor untuk masing-masing load cell
  Serial.print("Load Cell 1: ");
  Serial.print(GRAM1);
  Serial.println(" g");

  Serial.print("Load Cell 2: ");
  Serial.print(GRAM2);
  Serial.println(" g");

  Serial.print("Load Cell 3: ");
  Serial.print(GRAM3);
  Serial.println(" g");

  Serial.println();  // Baris kosong untuk memisahkan output
  delay(500);        // Delay 1/2 detik sebelum pembacaan berikutnya

  u8g2.clearBuffer();
  u8g2.drawRBox(25, 2, 80, 60, 8);
  u8g2.sendBuffer();
  delay(50);
  u8g2.drawRBox(15, 2, 104, 60, 8);
  u8g2.sendBuffer();
  delay(50);
  u8g2.drawRBox(5, 2, 114, 60, 8);
  u8g2.sendBuffer();
  delay(50);

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr);
  u8g2.setCursor(8, 17);
  u8g2.print("BERAT SAMPAH");
  u8g2.setCursor(8, 32);
  u8g2.print("ORGANIK       :" + String(GRAM1) + String(" gr"));
  u8g2.setCursor(9, 42);
  u8g2.print("ANORGANIK :" + String(GRAM2) + String(" gr"));
  u8g2.setCursor(9, 52);
  u8g2.print("LOGAM           :" + String(GRAM3) + String(" gr"));
  u8g2.drawFrame(5, 21, 114, 34);
  u8g2.sendBuffer();
}
