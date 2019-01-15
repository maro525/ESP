#include <ESP8266WiFi.h> // WiFiモジュールのライブラリ
#include <Arduino.h>     // LEDを光らせるため

// SPI通信するためのライブラリ
extern "C"{
#include <spi.h>
#include <spi_register.h>
}

// wifiのSSID
const char* ssid = "";
// wifiのパスワード
const char* password = "";

// iftttの設定
const char* host = "maker.ifttt.com";
const char* event = "esp-wroom-02";
const char* secretkey = "";

// センサーの値を入れる変数
int val0, val1;

// 起床間隔
const unsigned long WAKEUP_INTERVAL = 10 * 1000 * 1000; // 1分

// LEDのピン
int led_pin = 5;

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
void setup() {

    Serial.begin(115200);
    Serial.println();

    // LEDの設定
    pinMode(led_pin, OUTPUT);

    // SPI通信のセットアップ
    spi_init(HSPI);

    // LED光らせる
    digitalWrite(led_pin, HIGH);

    // 光センサーの値取得
    val0 = check(0);
    val1 = check(1);

    // IFTTTにアップロードする
    uploadIFTTT(val0, val1);

    // LEDを消す
    digitalWrite(led_pin, LOW);

    // ディープスリープに入る
    ESP.deepSleep(WAKEUP_INTERVAL, WAKE_RF_DEFAULT);
}




void loop() {

}













////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// SPI通信でセンサーのADコンバータに入力されているアナログピンの値を読む関数
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
uint32 check(int channel) {
    uint8 cmd = (0b11 << 3) | channel;

    const uint32 COMMAND_LENGTH = 5;
    const uint32 RESPONSE_LENGTH = 12;

    uint32 retval = spi_transaction(HSPI, 0, 0, 0, 0, COMMAND_LENGTH, cmd, RESPONSE_LENGTH, 0);

    retval = retval & 0x3FF;
    return retval;
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// IFTTTにアップロードする関数
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

void uploadIFTTT(int _v0, int _v1) {
    // WiFi接続
    WiFi.begin(ssid, password);
    const int httpPort = 80;
    while(WiFi.status() != WL_CONNECTED) {
        delay(100);
    }

    WiFiClient client;
    if(!client.connect(host, 80)) {
        return;
    }

    // IFTTTに送るURL
    String url = "trigger/";
    url += event;
    url += "/with/key/";
    url += secretkey;
    url += "?value1=";

    if(_v0 >= _v1){
        url += "front";
    } else {
        url += "back";
    }

    url += "&value2=";
    url += _v0;
    url += "&value3=";
    url += _v1;

    // サーバーに送る
    char sendData[256] = "";
    sprintf(sendData, "GET http://maker.ifttt.com/%s HTTP/1.1\r\nHost:maker.ifttt.com\r\nConnection: close\r\n\r\n", url.c_str());
    client.print(sendData);
    int timeout = millis() + 5000;
    while (client.available() == 0) {
        if (timeout - millis() < 0) {
            client.stop();
            return;
        }
    }
    while(client.available()){
        String line = client.readStringUntil('\r');
    }

    delay(10);
}
