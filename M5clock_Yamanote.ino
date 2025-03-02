#include <M5Stack.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <SD.h>
#include "AudioFileSourceSD.h"
#include "AudioFileSourceID3.h"
#include "AudioGeneratorMP3.h"
#include "AudioOutputI2S.h"

// Wi-Fi設定
const char* ssid = "ctc-xfczdu";
const char* password = "232c729c765d7";
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 9 * 3600;
const int daylightOffset_sec = 0;

// オーディオ設定
AudioGeneratorMP3 *mp3;
AudioFileSourceSD *file;
AudioOutputI2S *out;
AudioFileSourceID3 *id3;

// 特別な画像が表示されたかどうかを記録するフラグ
bool isSpecialImageShown = false;
unsigned long lastTriggerTime = 0;

// 明るさに応じた文字色を選択
uint16_t getTextColor(const char* filename) {
    // 背景画像の輝度を調べるために画像を読み込む
    File file = SD.open(filename);
    if (!file) {
        M5.Lcd.println("File not found!");
        return WHITE; // ファイルが見つからない場合は白に
    }
    
    M5.Lcd.drawJpgFile(SD, filename); // 画像を表示
    int width = M5.Lcd.width();
    int height = M5.Lcd.height();
    int pixelSampleCount = 10; // サンプリングするピクセル数

    int brightness = 0;
    int sampleWidth = width / pixelSampleCount;
    int sampleHeight = height / pixelSampleCount;

    for (int i = 0; i < pixelSampleCount; i++) {
        for (int j = 0; j < pixelSampleCount; j++) {
            int x = i * sampleWidth;
            int y = j * sampleHeight;
            uint16_t color = M5.Lcd.readPixel(x, y); // ここを変更
            uint8_t r = (color >> 11) & 0x1F;
            uint8_t g = (color >> 5) & 0x3F;
            uint8_t b = color & 0x1F;

            // RGBから輝度を計算（加重平均）
            brightness += (r * 0.299 + g * 0.587 + b * 0.114);
        }
    }

    brightness /= (pixelSampleCount * pixelSampleCount); // 平均輝度

    // 明るさに応じて文字色を設定
    if (brightness > 128) { // 輝度が高い場合、黒い文字
        return BLACK;
    } else { // 輝度が低い場合、白い文字
        return WHITE;
    }
}

void setup() {
    M5.begin();
    M5.Lcd.println("Connecting to WiFi...");
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        M5.Lcd.print(".");
    }
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

    if (!SD.begin(GPIO_NUM_4, SPI, 15000000)) {
        M5.Lcd.println("SD Card Mount Failed!");
        return;
    }

    M5.Lcd.fillScreen(BLACK);  // 画面をクリア
    displayJPEG("/clock_JPG/base.jpg"); // 起動直後に base.jpg を表示
}

void loop() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        M5.Lcd.println("Failed to obtain time");
        return;
    }

    // 時間に応じたファイル名を作成
    char mp3File[32], jpegFile[32];
    sprintf(mp3File, "/clock_mp3/%02d.mp3", timeinfo.tm_hour);
    sprintf(jpegFile, "/clock_JPG/%02d.jpg", timeinfo.tm_hour);

    // 背景画像に応じた文字色を設定
    //uint16_t textColor = getTextColor("/clock_JPG/base.jpg");

    // 毎時0分に特別な画像を表示し、音楽を再生（1分間の誤動作防止）
    if (timeinfo.tm_min == 0 && timeinfo.tm_sec == 0 && millis() - lastTriggerTime > 60000) {
        M5.Lcd.fillScreen(BLACK); // 特別な画像表示前に画面をクリア（時計を消す）
        displayJPEG(jpegFile);    // 画像を先に表示
        playMP3(mp3File);         // 画像表示後に音楽を再生
        isSpecialImageShown = true;
        lastTriggerTime = millis(); // 最後の実行時間を記録
    }

    // 特別な画像の表示中は時計を表示しない
    if (!isSpecialImageShown) {
        // 通常時の時計表示
        M5.Lcd.setCursor(40, 40);
        M5.Lcd.setTextSize(5);
        //M5.Lcd.setTextColor(textColor); 
        M5.Lcd.printf("%02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    }

    // 1分が経過したら時計表示を再開
    if (isSpecialImageShown && millis() - lastTriggerTime > 60000) {
        M5.Lcd.fillScreen(BLACK); // 通常画像に切り替える前にクリア
        displayJPEG("/clock_JPG/base.jpg"); // 通常の背景画像を表示
        isSpecialImageShown = false;
    }

    delay(1000);
}

void playMP3(const char* filename) {
    file = new AudioFileSourceSD(filename);
    id3 = new AudioFileSourceID3(file);
    out = new AudioOutputI2S(0, 1); // Output to builtInDAC
    out->SetOutputModeMono(true);
    out->SetGain(1.0);
    mp3 = new AudioGeneratorMP3();
    mp3->begin(id3, out);

    while (mp3->isRunning()) {
        if (!mp3->loop()) mp3->stop();
    }

    // メモリ解放
    delete mp3;
    delete out;
    delete id3;
    delete file;
}

void displayJPEG(const char* filename) {
    if (!SD.exists(filename)) {
        M5.Lcd.println("JPEG file not found!");
        return;
    }
    M5.Lcd.drawJpgFile(SD, filename);
}
