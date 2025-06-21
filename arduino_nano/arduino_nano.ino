#include "Neurona.h"
#include "model.h"

int mlpclass; 
const int pinor = 12; // Kapasitif
const int pinanor = 11; // Induktif
const int pinlog = 10; // IR

// Menginisialisasi MLP dengan bobot awal
MLP mlp(NET_INPUTS, NET_OUTPUTS, layerSizes, MLP::LOGISTIC, initW, true);

void setup() {
  Serial.begin(115200);
  pinMode(pinor, INPUT);
  pinMode(pinanor, INPUT);
  pinMode(pinlog, INPUT);
}

void loop() {
  int organik1 = digitalRead(pinor);
  int anorganik1 = digitalRead(pinanor);
  int logam1 = digitalRead(pinlog);
  
  double organik = organik1;
  double anorganik = anorganik1;
  double logam = logam1;

  netInput[1] = organik;
  netInput[2] = anorganik;
  netInput[3] = logam;

  Serial.print("Kapasitif: ");
  Serial.print(organik);
  Serial.print(" | Induktif: ");
  Serial.print(anorganik);
  Serial.print(" | IR: ");
  Serial.println(logam);

  // Melakukan inferensi
  int index = mlp.getActivation(netInput);
  mlpclass = index;
  //Serial.println(index);
  
  // Mengirimkan hasil klasifikasi ke ESP32
  Serial.println(Class[mlpclass]);  // Mengirimkan string hasil klasifikasi
  delay(3200);
}
