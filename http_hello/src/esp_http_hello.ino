#include <Arduino.h>

#include <ESP8266WiFi.h>

// wifiのSSID
const char* ssid = "";
// wifiのパスワード
const char* password = "";

//ブラウザからの初回HTTPレスポンス完了したかどうかのフラグ
boolean Ini_html_on = false;

WiFiServer server(80);
WiFiClient client;

void setup() {
  Serial.begin(115200);
  // Connect to WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");

  // Start the server
  server.begin();
  Serial.println("Server started");

  // Print the IP address
  Serial.println(WiFi.localIP());

}

//************メインループ********************************
void loop() {
  if(Ini_html_on == false){
      Ini_HTTP_Response();
  }else if(client.available()){
    Serial.print(client.read());
  }
  delay(1);//これは重要かも。これがないと動作しないかも。
}

//*****初回ブラウザからのGET要求によるHTMLタグ吐き出しHTTPレスポンス*******
void Ini_HTTP_Response()
{
  client = server.available();//クライアント生成
  delay(1);
  String req;

  while(client){
    if(client.available()){
      req = client.readStringUntil('\n');
      Serial.println(req);
      if (req.indexOf("GET / HTTP") >= 0 || req.indexOf("GET /favicon") >= 0){//ブラウザからリクエストを受信したらこの文字列を検知する
        //Google Chromeの場合faviconリクエストが来るのでそれも検出する
        Serial.println("-----from Browser FirstTime HTTP Request---------");
        Serial.println(req);
        //ブラウザからのリクエストで空行（\r\nが先頭になる）まで読み込む
        while(req.indexOf("\r") != 0){
          req = client.readStringUntil('\n');//\nまで読み込むが\n自身は文字列に含まれず、捨てられる
          Serial.println(req);
        }
        req = "";
        delay(10);//10ms待ってレスポンスをブラウザに送信

        //メモリ節約のため、Fマクロで文字列を囲う
        //普通のHTTPレスポンスヘッダ
        client.print(F("HTTP/1.1 200 OK\r\n"));
        client.print(F("Content-Type:text/html\r\n"));
        client.print(F("Connection:close\r\n\r\n"));//１行空行が必要
        //ここからブラウザ表示のためのHTML JavaScript吐き出し
        client.println(F("<!DOCTYPE html>"));
        client.println(F("<html>"));
        client.println(F("<font size=30>"));
        client.println(F("Hello World"));
        client.println(F("</html>\r\n"));

        delay(1);//これが重要！これが無いと切断できないかもしれない。
        client.stop();//一旦ブラウザとコネクション切断する。

        Serial.println("\nGET HTTP client stop--------------------");
        req = "";
        Ini_html_on = false;  //一回切りの接続にしたい場合、ここをtrueにする。
      }
    }
  }
}
