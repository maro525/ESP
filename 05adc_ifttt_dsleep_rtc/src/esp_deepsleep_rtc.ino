////////////////////////////////////////////////////////////////////////////////
////インクルード//////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

#include <ESP8266WiFi.h> // WiFiモジュールのライブラリ
#include <Arduino.h>

extern "C"{
    // SPI通信するためのライブラリ
#include <spi.h>
#include <spi_register.h>
#include "user_interface.h"
#include "sntp.h"
#include "mem.h"
}

////////////////////////////////////////////////////////////////////////////////
////変数設定//////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

// wifiのSSID
const char* ssid = "";
// wifiのパスワード
const char* password = "";

// iftttの設定
const char* host = "maker.ifttt.com";
const char* event = "esp-wroom-02";
const char* secretkey = "";

// 起床間隔
const unsigned long WAKEUP_INTERVAL = 10 * 1000 * 1000; // 1分

// LEDのピン
int led_pin = 5;

////////////////////////////////////////////////////////////////////////////////
////データ周りの変数設定///////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

// RTCメモリーのアドレス
#define kOffSet_CRC 65

#pragma pack(push)
#pragma pack(4)

#define kMaxDataArea (512 - 4 - sizeof(int))

typedef struct {
    bool front;
    bool first;
    uint16_t light_front;
    uint16_t light_back;
} Data;

typedef struct {
    int crc;
    union Uni {
        Data data;
        char buffer[kMaxDataArea];
    } uni;
} Storage;
#pragma pack(pop)

Storage storage;


////////////////////////////////////////////////////////////////////////////////
////メイン関数////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

void setup() {
    pinMode(13, OUTPUT);

    Serial.begin(115200);
    Serial.println("Initializing...");

    // LEDの設定
    pinMode(led_pin, OUTPUT);

    // SPI通信のセットアップ
    spi_init(HSPI);

    // LED光らせる
    digitalWrite(led_pin, HIGH);
    Serial.println("LED up");

    // 光センサーの値取得
    int sensor_front = check(0);
    int sensor_back = check(1);
    Serial.print("light_front = ");
    Serial.println(sensor_front);
    Serial.print("light_back = ");
    Serial.println(sensor_back);

    // 初めての実行ならバッファを初期化する
    readStorageAndInitIfNeeded();

    // 一旦表裏情報をFLAGに保存
    bool front_tmp;
    int front_bool_to_bit;
    if(sensor_front > sensor_back) {
        front_tmp = true;
        front_bool_to_bit = 1;
    } else if(sensor_back > sensor_front) {
        front_tmp = false;
        front_bool_to_bit = 0;
    } else {
        front_tmp = NULL;
        front_bool_to_bit = 2;
    }

    // データ読み込み
    Data *pData = &storage.uni.data;
    pData->light_front = sensor_front;
    pData->light_back = sensor_back;

    // IFTTTに送信するかしないかのFLAG
    bool send;
    if(front_tmp == NULL){
        send = false;
    } else {
        if (front_tmp != pData->front) {
            send = true;
        } else {
            send = false;
        }
    }

    // rtcメモリーのデータ更新
    pData->front = front_tmp;

    // データ送信
    if(!pData->first){
        if(send) {
            uploadIFTTT(front_bool_to_bit, sensor_front, sensor_back);
        }
    } else {
        uploadIFTTT(front_bool_to_bit, sensor_front, sensor_back);
    }

    writeStorage();

    digitalWrite(led_pin, LOW);
    Serial.println("LED down");

    // WiFiをオフでディープスリープから復帰
    ESP.deepSleep(WAKEUP_INTERVAL, WAKE_RF_DEFAULT);
    Serial.println("goodbye......");

}

void loop() {
}















////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////関数///////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////





////////////////////////////////////////////////////////////////////////////////
////Storageを読み込む. CRCが不一致なら初期化/////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

bool readStorageAndInitIfNeeded() {
    Serial.print("storage size = ");
    Serial.println(sizeof(storage));
    bool ok = system_rtc_mem_read(kOffSet_CRC, &storage, sizeof(storage));
    if(!ok) {
        Serial.println("readStorageAndInitIfNeeded : mem read fail");
        return ok;
    }

    int crc = calcCRC();
    Serial.print("crc = ");
    Serial.print(crc, HEX);
    Serial.print(", storage crc = ");
    Serial.println(storage.crc, HEX);
    if(crc != storage.crc) {
        Serial.println("crc mismatch");
        memset(storage.uni.buffer, 0, sizeof(storage.uni.buffer));
        storage.uni.data.first = true;
        crc = calcCRC();
        Serial.print("new crc = ");
        Serial.println(crc, HEX);
        storage.crc = crc;
        ok = system_rtc_mem_write(kOffSet_CRC, &storage, sizeof(storage));
        if(!ok) {
            Serial.println("readStorageAndInitIfNeeded : write fail");
        }
    } else {
        storage.uni.data.first = false;
    }

    return ok;
}

bool writeStorage() {
    storage.crc = calcCRC();
    bool ok = system_rtc_mem_write(kOffSet_CRC, &storage, sizeof(storage));
    if (!ok) {
        Serial.println("Error : writeStorage : system_rtc_mem_write fail");
    }

    return ok;
}


int calcCRC()
{
  char *ptr = storage.uni.buffer;
  int   count = sizeof(storage.uni.buffer);
  Serial.print("calc crc counter = ");
  Serial.println(count);

  int  crc;
  char i;
  crc = 0;
  while (--count >= 0)
  {
    crc = crc ^ (int) * ptr++ << 8;
    i = 8;
    do
    {
      if (crc & 0x8000)
        crc = crc << 1 ^ 0x1021;
      else
        crc = crc << 1;
    } while (--i);
  }
  return (crc);
}




////////////////////////////////////////////////////////////////////////////////
// SPI通信でセンサーのADコンバータに入力されているアナログピンの値を読む関数
////////////////////////////////////////////////////////////////////////////////
uint32 check(int channel) {
    // mcp3008
    // uint8 cmd = (0b11 << 3) | channel;　
    // mcp3002
    uint8 cmd = channel == 0 ? 0b1101 : 0b1111;

    const uint32 COMMAND_LENGTH = 5;
    const uint32 RESPONSE_LENGTH = 12;

    uint32 retval = spi_transaction(HSPI, 0, 0, 0, 0, COMMAND_LENGTH, cmd, RESPONSE_LENGTH, 0);

    retval = retval & 0x3FF; // mask to 10-bit value
    return retval;
}




////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// IFTTTにアップロードする関数
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

void uploadIFTTT(int rec0, int rec1, int rec2) {

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
    Serial.println("wifi connected ........");

    // IFTTTに送るURL
    String url = "trigger/";
    url += event;
    url += "/with/key/";
    url += secretkey;
    url += "?value1=";

    if(rec0 == 1) {
        url += "front";
    } else if(rec0 == 0) {
        url += "back";
    } else if (rec0 == 2) {
        url += "undefined";
    }

    url += "&value2=";
    url += rec1;
    url += "&value3=";
    url += rec2;

    // サーバーに送る
    char sendData[256] = "";
    sprintf(sendData, "GET http://maker.ifttt.com/%s HTTP/1.1\r\nHost:maker.ifttt.com\r\nConnection: close\r\n\r\n", url.c_str());
    client.print(sendData);
    Serial.println("send to ifttt...............");
    Serial.print("url = ");
    Serial.println(url);
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
