#define BLYNK_TEMPLATE_ID "TMPL6vVQXINP3"
#define BLYNK_TEMPLATE_NAME "Smart Water"
#define BLYNK_AUTH_TOKEN "M644Q9PGLxyz5lxEDD1-PIrsh7XMBwnn"

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <LiquidCrystal_I2C.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include <SD.h>
#include <SPI.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <BlynkSimpleEsp32.h>

#define FLOW_SENSOR_PIN 26   // Pin untuk sensor waterflow
#define PULSES_PER_LITER 450 // Pulsa per liter untuk sensor YF-S201
#define SD_CS_PIN 5          // Pin CS untuk microSD card

// Inisialisasi LCD (20x4)
LiquidCrystal_I2C lcd(0x27, 20, 4);

// WiFi Credentials
const char* ssid = "fauzi";
const char* password = "muntilan";

// Telegram BOT
#define BOTtoken "7527693914:AAHTVMdaJYKI3bg-Io2HUhyi3PpY2uLHD_w" 
#define CHAT_ID "5994652575"

WiFiClientSecure client;
UniversalTelegramBot bot(BOTtoken, client);
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "asia.pool.ntp.org", 7 * 3600, 10000); // UTC+7 offset with 10-second update interval

// Variabel untuk sensor waterflow
volatile int pulseCount = 0; 
float flowRate = 0;
float totalLiters = 0;
float costPerM3 = 3500.00;  // Harga per m³
float totalCost = 0;

File dataFile;  // Untuk manajemen file di microSD

// Fungsi untuk menghitung pulsa dari sensor aliran
void IRAM_ATTR pulseCounter() {
  pulseCount++;
}

// Fungsi untuk membaca data dari microSD saat startup
void readFromSD() {
  dataFile = SD.open("/data.txt");
  if (dataFile) {
    totalLiters = dataFile.readStringUntil('\n').toFloat();
    totalCost = dataFile.readStringUntil('\n').toFloat();
    dataFile.close();
  }
}

// Fungsi untuk menyimpan data ke microSD
void writeToSD() {
  dataFile = SD.open("/data.txt", FILE_WRITE); // Tulis data dan menimpa file lama
  if (dataFile) {
    dataFile.println(totalLiters);
    dataFile.println(totalCost);
    dataFile.close();
  }
}

// Setup Wi-Fi dan komponen lainnya
void setup() {
  Serial.begin(115200);

  // Inisialisasi LCD
  Wire.begin(21, 22); // SDA = GPIO21, SCL = GPIO22
  lcd.init();
  lcd.backlight();

  // Tampilkan pesan sambutan "Smart Water" selama 1 detik 
  lcd.setCursor(4, 1);    // Mengatur posisi ke tengah layar (20x4)
  lcd.print("Smart Water");
  delay(1000);            // Tahan pesan selama 2 detik 
  lcd.clear();            // Hapus pesan setelah sambutan

  // Inisialisasi SD card
  if (!SD.begin(SD_CS_PIN)) {
    Serial.println("SD Card initialization failed!");
    return;
  }

  // Inisialisasi Wi-Fi
  WiFi.begin(ssid, password);
  unsigned long wifiStart = millis();
  bool wifiConnected = false;
  while (millis() - wifiStart < 10000) { // Timeout 10 detik
    if (WiFi.status() == WL_CONNECTED) {
      wifiConnected = true;
      break;
    }
    delay(500);
    Serial.print(".");
  }
  if (wifiConnected) {
    Serial.println("\nWiFi connected.");
    client.setCACert(TELEGRAM_CERTIFICATE_ROOT); // Setup koneksi aman untuk Telegram
    bot.sendMessage(CHAT_ID, "Bot started up", "");
  } else {
    Serial.println("\nWiFi not connected.");
  }

  // Setup NTP Client
  timeClient.begin();

  readFromSD(); // Baca data dari microSD

  // Setup sensor aliran air
  pinMode(FLOW_SENSOR_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(FLOW_SENSOR_PIN), pulseCounter, FALLING);

  // Menampilkan pesan awal di LCD
  lcd.setCursor(0, 0);
  lcd.print("Biaya: ");
  lcd.setCursor(0, 1);
  lcd.print("Total Debit:");
  lcd.setCursor(0, 2);
  lcd.print("Laju: ");
  lcd.setCursor(0, 3);
  lcd.print("Pulse Count:");

  // Inisialisasi Blynk
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, password);
}

void loop() {
  Blynk.run();

  // Update NTP time
  timeClient.update();
  Serial.print("Tanggal dan Waktu: ");
  Serial.println(timeClient.getFormattedTime());

  static unsigned long lastTime = 0;
  unsigned long currentTime = millis();

  // Hitung debit air setiap 1 detik
  if (currentTime - lastTime >= 1000) {  // Setiap 1 detik
    // Hitung laju aliran
    flowRate = ((float)pulseCount / PULSES_PER_LITER);

    // Tambahkan total liter
    totalLiters += flowRate;

    // Hitung biaya
    totalCost = (totalLiters / 1000) * costPerM3;

    // Reset pulsa
    pulseCount = 0;
    lastTime = currentTime;

    // Tampilkan di LCD
    lcd.setCursor(7, 0);     
    lcd.print("Rp ");
    lcd.print(totalCost, 2); 

    lcd.setCursor(13, 1);    
    lcd.print(totalLiters, 2);
    lcd.print(" L");  // Tambahkan satuan liter

    lcd.setCursor(7, 2);    
    lcd.print(flowRate, 2); 

    lcd.setCursor(13, 3);    
    lcd.print(pulseCount); 

    // Tampilkan di Serial Monitor
    Serial.print("Total Biaya: Rp ");
    Serial.print(totalCost, 2);
    Serial.print(" | Total Debit: ");
    Serial.print(totalLiters, 2);
    Serial.println(" L");

    // Simpan ke microSD
    writeToSD();

    // Kirim data ke Blynk
    Blynk.virtualWrite(V1, totalLiters);    // Kirim Total Debit ke Chart (V1)
    Blynk.virtualWrite(V2, flowRate);       // Kirim Laju Aliran ke Gauge (V2)
    Blynk.virtualWrite(V3, totalCost);      // Kirim Total Biaya ke Label (V3)
  }

  // Cek apakah ada pesan dari bot Telegram
  int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
  for (int i = 0; i < numNewMessages; i++) {
    String chat_id = bot.messages[i].chat_id;
    String text = bot.messages[i].text;

    // Command "/status" to display current status
    if (text == "/status") {
      String message = "Tanggal dan Waktu: " + timeClient.getFormattedTime() + "\n";
      message += "Total Debit: " + String(totalLiters, 2) + " L\n";
      message += "Total Biaya: Rp " + String(totalCost, 2);
      bot.sendMessage(chat_id, message, "");
    }
    
    // Command "/reset" untuk menghapus data di SD card
    if (text == "/reset") {
      // Hapus data di SD Card
      if (SD.exists("/data.txt")) {
        SD.remove("/data.txt");
        // Setel ulang variabel yang menyimpan data debit dan biaya
        totalLiters = 0;
        totalCost = 0;
        bot.sendMessage(chat_id, "Data telah direset di kartu SD dan variabel diatur ulang.", "");
      } else {
        bot.sendMessage(chat_id, "Data tidak ditemukan di kartu SD.", "");
      }
    }
  }
}
