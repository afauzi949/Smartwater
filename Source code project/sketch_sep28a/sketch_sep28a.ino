#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <SD.h>  // Tambahkan library SD untuk manajemen microSD card

#define FLOW_SENSOR_PIN 26   // Pin di mana sensor YF-S201 terhubung
#define PULSES_PER_LITER 450 // Pulsa per liter (nilai umum untuk YF-S201, bisa disesuaikan)
#define SD_CS_PIN 5          // Pin CS untuk modul microSD card

// Inisialisasi alamat I2C dan ukuran LCD (20x4)
LiquidCrystal_I2C lcd(0x27, 20, 4);

const char* ssid = "naf";        // Nama SSID Wi-Fi
const char* password = "nafillasuksesduniaakhiratamin"; // Kata sandi Wi-Fi

const char* telegramApiToken = "7527693914:AAEGYcfsZdgyZgtvQNr9uGKiqC93LDACi68";  // Token API Telegram
const char* chatID = "5994652575";  // ID obrolan bot Telegram (chat_id)

volatile int pulseCount = 0;    // Variabel untuk menghitung pulsa dari sensor
float flowRate = 0;             // Laju aliran dalam liter per detik
float totalLiters = 0;          // Jumlah total liter air yang mengalir
float costPerM3 = 3500.00;      // Harga per m³ (3500 Rupiah per m³)
float totalCost = 0;            // Biaya total berdasarkan konsumsi air

WiFiClientSecure client;  // Koneksi HTTPS yang aman
File dataFile;            // File untuk menyimpan data di microSD

// Fungsi untuk membaca data dari microSD saat startup
void readFromSD() {
  dataFile = SD.open("/data.txt");
  if (dataFile) {
    String data = dataFile.readStringUntil('\n');
    totalLiters = data.toFloat();
    data = dataFile.readStringUntil('\n');
    totalCost = data.toFloat();
    dataFile.close();
    Serial.println("Data from SD card:");
    Serial.println("Total Liters: " + String(totalLiters));
    Serial.println("Total Cost: Rp " + String(totalCost));
  } else {
    Serial.println("Failed to open data file from SD card.");
  }
}

// Fungsi untuk menyimpan data ke microSD
void writeToSD() {
  dataFile = SD.open("/data.txt", FILE_WRITE);
  if (dataFile) {
    dataFile.println(totalLiters);
    dataFile.println(totalCost);
    dataFile.close();
    Serial.println("Data written to SD card.");
  } else {
    Serial.println("Error opening data file for writing.");
  }
}

// Fungsi yang dipanggil setiap kali pulsa terdeteksi dari sensor
void pulseCounter() {
  pulseCount++;
}

// Fungsi untuk mengirim pesan ke bot Telegram
void sendTelegramMessage(String message) {
  Serial.println("Attempting to connect to Telegram API...");

  if (client.connect("api.telegram.org", 443)) {
    Serial.println("Connected to Telegram API!");

    String url = "/bot" + String(telegramApiToken) + "/sendMessage?chat_id=" + String(chatID) + "&text=" + message;
    Serial.println("Sending request to: " + url);
    
    client.print(String("GET ") + url + " HTTP/1.1\r\n" +
                 "Host: api.telegram.org\r\n" +
                 "Connection: close\r\n\r\n");

    // Baca respons dari server
    while (client.available()) {
      String line = client.readStringUntil('\r');
      Serial.print(line);  // Cetak respons dari server
    }

    client.stop(); // Tutup koneksi
    Serial.println("Message sent successfully.");
  } else {
    Serial.println("Failed to connect to Telegram API.");
  }
}

void setup() {
  // Inisialisasi Serial Monitor untuk debugging
  Serial.begin(115200);

  // Inisialisasi Wi-Fi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnected to WiFi");
  } else {
    Serial.println("Failed to connect to WiFi");
    return;
  }

  // Inisialisasi LCD
  Wire.begin(21, 22);  // SDA = GPIO21, SCL = GPIO22
  lcd.init();
  lcd.backlight();

  // Inisialisasi SD card
  if (!SD.begin(SD_CS_PIN)) {
    Serial.println("SD Card initialization failed!");
    return;
  }
  Serial.println("SD Card initialized.");
  
  // Baca data dari SD card jika tersedia
  readFromSD();

  // Setup pin sensor dan interrupt untuk mendeteksi pulsa
  pinMode(FLOW_SENSOR_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(FLOW_SENSOR_PIN), pulseCounter, FALLING);

  // Tampilkan pesan awal di LCD
  lcd.setCursor(0, 0);
  lcd.print("Biaya: ");
  lcd.setCursor(0, 1);
  lcd.print("Total Debit:");
  lcd.setCursor(0, 2);
  lcd.print("Laju: ");
  lcd.setCursor(0, 3);
  lcd.print("Pulse Count:");
  
  // Atur waktu koneksi aman
  client.setInsecure();  // Gunakan koneksi aman tanpa validasi sertifikat
}

unsigned long lastMessageTime = 0; // Variabel untuk menyimpan waktu terakhir pesan dikirim
const unsigned long messageInterval = 3000; // Interval pengiriman pesan dalam milidetik (3 detik)

void loop() {
  static unsigned long lastTime = 0;
  unsigned long currentTime = millis();

  // Hitung debit air setiap 1 detik
  if (currentTime - lastTime >= 1000) {
    // Hitung laju aliran dalam liter per detik
    flowRate = ((float)pulseCount / PULSES_PER_LITER);

    // Tambahkan ke total liter
    totalLiters += flowRate;

    // Hitung total biaya
    totalCost = (totalLiters / 1000) * costPerM3; // Konversi total liter ke m³ dan hitung biaya

    // Reset hitungan pulsa setelah 1 detik
    pulseCount = 0;
    lastTime = currentTime;

    // Tampilkan di LCD
    lcd.setCursor(7, 0);     
    lcd.print("Rp ");
    lcd.print(totalCost, 2); 

    lcd.setCursor(13, 1);    
    lcd.print(totalLiters, 2); 

    lcd.setCursor(7, 2);    
    lcd.print(flowRate, 2); 

    lcd.setCursor(13, 3);    
    lcd.print(pulseCount); 

    // Simpan data ke SD card
    writeToSD();

    // Kirim pesan ke bot Telegram setiap 3 detik
    if (currentTime - lastMessageTime >= messageInterval) {
      String message = "Total Debit: " + String(totalLiters, 2) + " L\nTotal Biaya: Rp " + String(totalCost, 2);
      sendTelegramMessage(message);
      lastMessageTime = currentTime; // Update waktu terakhir pesan dikirim
    }
  }
}
