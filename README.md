# ArduinoWaste-e

Sistem klasifikasi dan pemantauan jenis sampah berbasis **Arduino Nano** dan **ESP32**. Proyek ini menggabungkan teknik klasifikasi berbasis Artificial Neural Network (ANN) dengan komunikasi serial antar mikrokontroler, serta pengiriman data ke **MySQL** atau **Firebase** sebagai backend database. Pemilihan database dapat menggunakan MySQL atau Firebase sesuai kebutuhan.  

---

## Sistem Klasifikasi Sampah

### Komponen Utama:
- **Arduino Nano**: untuk klasifikasi jenis sampah.
- **ESP32**: untuk mengirimkan data klasifikasi dan pembacaan sensor ke database.

### Sensor Input:
- **IR sensor** untuk mendeteksi sampah logam
- **Proximity capacitive sensor** untuk mendeteksi sampah organik
- **Proximity inductive sensor** untuk mendeteksi sampah anorganik

### Proses Klasifikasi:
1. **Data sensor** dibaca oleh Arduino Nano.
2. Data tersebut diproses oleh **Artificial Neural Network (ANN)** yang telah dilatih sebelumnya menggunakan metode **Multi-Layer Perceptron (MLP)**.
3. Model ANN dilatih menggunakan [MLP Topology Workbench](http://www.moretticb.com/MTW/).
4. Hasil training dimasukkan ke dalam file `model.h` yang berisi bobot dan struktur jaringan.
5. Layer yang digunakan ada 3 input, 9 hidden layer, dan 4 output. 
