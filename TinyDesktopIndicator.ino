/* *****************************************************************
 * 
 * TinyDesktopIndicator-OLED
 *    小型桌面显示器OLED版本
 * 
 * 原  作  者：Misaka & 微车游
 * 修      改：JimHan
 * 最后更改日期：2022.04.22
 * 更 新 日 志：V1.0 修改显示为OLED，删除微信配网，关闭DHT传感器，UI大改以适应0.96'OLED
 *             V1.1 更新了时间字体，优化了排版，更新网页配置面板为SDI新版外观。解决了时间字体重叠问题。
 *             V1.2 删除了所有TFT-spi依赖项目，加上时间冒号闪动，天气修改为3秒切换，优化web控制台显示，现在可以显示当前设定值。
 * 
 * 引 脚 分 配：
 *              SCL   GPIO14
 *              SDA   GPIO13
 *             
 * 
 * *****************************************************************/
#define Version  "TDI-OLED V1.2.1"
/* *****************************************************************
 *  库文件、头文件
 * *****************************************************************/
#include "ArduinoJson.h"
#include <TimeLib.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <WiFiUdp.h>
#include <ESP8266mDNS.h>
#include <EEPROM.h>

// ?u8g2 OLED显示库
#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>
U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2(U8G2_R0, /* clock=*/ SCL, /* data=*/ SDA, /* reset=*/ U8X8_PIN_NONE);   // All Boards without Reset of the Display

//#include "number.h"
//#include "weathernum.h"

/* *****************************************************************
 *  配置使能位
 * *****************************************************************/
//Web服务器使能标志位----打开后将无法使用wifi休眠功能。
#define WebSever_EN  1


//注意，此版本中的DHT11传感器和太空人图片选择可以通过web网页设置来进行选择，无需通过使能标志来重新编译。
//设定DHT11温湿度传感器使能标志
#define DHT_EN  0

#include <WiFiManager.h>
//WiFiManager 参数
WiFiManager wm; // global wm instance
// WiFiManagerParameter custom_field; // global param ( for non blocking w params )

#if DHT_EN
#include "DHT.h"
#define DHTPIN  12
#define DHTTYPE DHT11
DHT dht(DHTPIN,DHTTYPE);
#endif

/* *****************************************************************
 *  字库、图片库 BANNED
 * *****************************************************************/

//#include "font/ZdyLwFont_20.h"
//#include "img/misaka.h"
//#include "img/temperature.h"
//#include "img/humidity.h"

/* *****************************************************************
 *  参数设置
 * *****************************************************************/

struct config_type
{
  char stassid[32];//定义配网得到的WIFI名长度(最大32字节)
  char stapsw[64];//定义配网得到的WIFI密码长度(最大64字节)
};

//---------------修改此处""内的信息--------------------
//如开启WEB配网则可不用设置这里的参数，前一个为wifi ssid，后一个为密码
config_type wificonf ={{""},{""}};


int updateweater_time = 10; //天气更新时间  X 分钟
int LCD_Rotation = 0;   //LCD屏幕方向
int LCD_BL_PWM = 50;//屏幕亮度0-100，默认50
String cityCode = "0";  //天气城市代码 默认自动 长沙:101250101株洲:101250301衡阳:101250401
//----------------------------------------------------

//其余状态标志位
uint8_t Wifi_en = 1; //wifi状态标志位  1：打开    0：关闭
uint8_t UpdateWeater_en = 0; //更新时间标志位
int prevTime = 0;       //滚动显示更新标志位
int DHT_img_flag = 0;   //DHT传感器使用标志位


//EEPROM参数存储地址位
int BL_addr = 1;//被写入数据的EEPROM地址编号  1亮度
int Ro_addr = 2; //被写入数据的EEPROM地址编号  2 旋转方向
int DHT_addr = 3;//3 DHT使能标志位
int UpWeT_addr = 4; //4 更新时间记录
int CC_addr = 10;//被写入数据的EEPROM地址编号  10城市
int wifi_addr = 30; //被写入数据的EEPROM地址编号  20wifi-ssid-psw

time_t prevDisplay = 0;       //显示时间显示记录
unsigned long weaterTime = 0; //天气更新时间记录
String SMOD = "";//串口数据存储


/*** Component objects ***/
//Number      dig;
//WeatherNum  wrat;


uint32_t targetTime = 0;

int tempnum = 0;   //温度百分比
int huminum = 0;   //湿度百分比
// int tempcol =0xffff;   //温度显示颜色
// int humicol =0xffff;   //湿度显示颜色

//Web网站服务器
ESP8266WebServer server(80);// 建立esp8266网站服务器对象


//NTP服务器参数
static const char ntpServerName[] = "ntp6.aliyun.com";
const int timeZone = 8;     //东八区

//wifi连接UDP设置参数
WiFiUDP Udp;
WiFiClient wificlient;
unsigned int localPort = 8000;
float duty=0;


//函数声明
time_t getNtpTime();
void digitalClockDisplay(int reflash_en);
void printDigits(int digits);
String num2str(int digits);
void sendNTPpacket(IPAddress &address);
void LCD_reflash(int en);
void savewificonfig();
void readwificonfig();
void deletewificonfig();
#if WebSever_EN
void Web_Sever_Init();
void Web_Sever();
#endif
void saveCityCodetoEEP(int * citycode);
void readCityCodefromEEP(int * citycode);

/* *****************************************************************
 *  函数
 * *****************************************************************/
//读取保存城市代码
void saveCityCodetoEEP(int * citycode)
{
  for(int cnum=0;cnum<5;cnum++)
  {
    EEPROM.write(CC_addr+cnum,*citycode%100);//城市地址写入城市代码
    EEPROM.commit();//保存更改的数据
    *citycode = *citycode/100;
    delay(5);
  }
}


void readCityCodefromEEP(int * citycode)
{
  for(int cnum=5;cnum>0;cnum--)
  {          
    *citycode = *citycode*100;
    *citycode += EEPROM.read(CC_addr+cnum-1); 
    delay(5);
  }
}


//wifi ssid，psw保存到eeprom
void savewificonfig()
{
  //开始写入
  uint8_t *p = (uint8_t*)(&wificonf);
  for (int i = 0; i < sizeof(wificonf); i++)
  {
    EEPROM.write(i + wifi_addr, *(p + i)); //在闪存内模拟写入
  }
  delay(10);
  EEPROM.commit();//执行写入ROM
  delay(10);
}


//删除原有eeprom中的信息
void deletewificonfig()
{
  config_type deletewifi ={{""},{""}};
  uint8_t *p = (uint8_t*)(&deletewifi);
  for (int i = 0; i < sizeof(deletewifi); i++)
  {
    EEPROM.write(i + wifi_addr, *(p + i)); //在闪存内模拟写入
  }
  delay(10);
  EEPROM.commit();//执行写入ROM
  delay(10);
}


//从eeprom读取WiFi信息ssid，psw
void readwificonfig()
{
  uint8_t *p = (uint8_t*)(&wificonf);
  for (int i = 0; i < sizeof(wificonf); i++)
  {
    *(p + i) = EEPROM.read(i + wifi_addr);
  }
  // EEPROM.commit();
  // ssid = wificonf.stassid;
  // pass = wificonf.stapsw;
  Serial.printf("Read WiFi Config.....\r\n");
  Serial.printf("SSID:%s\r\n",wificonf.stassid);
  Serial.printf("PSW:%s\r\n",wificonf.stapsw);
  Serial.printf("Connecting.....\r\n");
}


// todo 进度条函数-修改完成待测试

byte loadNum = 6;
void loading(byte delayTime)//绘制进度条
{
  // clk.setColorDepth(8);
  
  // clk.createSprite(200, 100);//创建窗口
  // clk.fillSprite(0x0000);    //填充率

  u8g2.drawFrame(10, 25, 108, 15);   //空心直角矩形
  u8g2.drawBox(12, 27, loadNum, 11); //实心直角矩形
  u8g2.setFont(u8g2_font_wqy13_t_gb2312);
  u8g2.drawUTF8(28,18, "连接到WiFi..."); 
  u8g2.setFont(u8g2_font_mozart_nbp_tf);
  u8g2.drawUTF8(37,50, Version); //显示版本号信息
  u8g2.sendBuffer();          // transfer internal memory to the display

  // clk.drawRoundRect(0,0,200,16,8,0xFFFF);       //空心圆角矩形
  // clk.fillRoundRect(3,3,loadNum,10,5,0xFFFF);   //实心圆角矩形
  // clk.setTextDatum(CC_DATUM);   //设置文本数据
  // clk.setTextColor(TFT_GREEN, 0x0000); 
  // clk.drawString("Connecting to WiFi......",100,40,2);
  // clk.setTextColor(TFT_WHITE, 0x0000); 
  // clk.drawRightString(Version,180,60,2);
  // clk.pushSprite(20,120);  //窗口位置
  // clk.setTextDatum(CC_DATUM);
  // clk.setTextColor(TFT_WHITE, 0x0000); 
  // clk.pushSprite(130,180);
  
  // clk.deleteSprite();
  loadNum += 1;
  delay(delayTime);
}

// !已停止使用-DHT11
#if DHT_EN
//外接DHT11传感器，显示数据
//void IndoorTem()
//{
//  float t = dht.readTemperature();
//  float h = dht.readHumidity();
//  String s = "内温";
//   /***绘制相关文字***/
//  clk.setColorDepth(8);
//  clk.loadFont(ZdyLwFont_20);
//  
//  //位置
//  clk.createSprite(58, 30);
//  clk.fillSprite(bgColor);
//  clk.setTextDatum(CC_DATUM);
//  clk.setTextColor(TFT_WHITE, bgColor);
//  clk.drawString(s,29,16);
//  clk.pushSprite(172,150);
//  clk.deleteSprite();
//  
//  //温度
//  clk.createSprite(60, 24); 
//  clk.fillSprite(bgColor);
//  clk.setTextDatum(CC_DATUM);
//  clk.setTextColor(TFT_WHITE, bgColor); 
//  clk.drawFloat(t,1,20,13);
////  clk.drawString(sk["temp"].as<String>()+"℃",28,13);
//  clk.drawString("℃",50,13);
//  clk.pushSprite(170,184);
//  clk.deleteSprite();
//  
//  //湿度
//  clk.createSprite(60, 24); 
//  clk.fillSprite(bgColor);
//  clk.setTextDatum(CC_DATUM);
//  clk.setTextColor(TFT_WHITE, bgColor); 
////  clk.drawString(sk["SD"].as<String>(),28,13);
//  clk.drawFloat(h,1,20,13);
//  clk.drawString("%",50,13);
//  //clk.drawString("100%",28,13);
//  clk.pushSprite(170,214);
//  clk.deleteSprite();
//}
#endif


// todo 串口调试设置函数 - 注释掉了屏幕旋转代码
void Serial_set()
{
  String incomingByte = "";
  if(Serial.available()>0)
  {
    
    while(Serial.available()>0)//监测串口缓存，当有数据输入时，循环赋值给incomingByte
    {
      incomingByte += char(Serial.read());//读取单个字符值，转换为字符，并按顺序一个个赋值给incomingByte
      delay(2);//不能省略，因为读取缓冲区数据需要时间
    }    
    if(SMOD=="0x01")//设置1亮度设置
    {
      int LCDBL = atoi(incomingByte.c_str());//int n = atoi(xxx.c_str());//String转int
      if(LCDBL>=0 && LCDBL<=100)
      {
        EEPROM.write(BL_addr, LCDBL);//亮度地址写入亮度值
        EEPROM.commit();//保存更改的数据
        delay(5);
        LCD_BL_PWM = EEPROM.read(BL_addr); 
        delay(5);
        SMOD = "";
        Serial.printf("亮度调整为：");
        u8g2.setContrast(LCD_BL_PWM*2.55);
        Serial.println(LCD_BL_PWM);
        Serial.println("");
      }
      else
        Serial.println("亮度调整错误，请输入0-100");
    } 
    if(SMOD=="0x02")//设置2地址设置
    {
      int CityCODE = 0;
      int CityC = atoi(incomingByte.c_str());//int n = atoi(xxx.c_str());//String转int
      if(CityC>=101000000 && CityC<=102000000 || CityC == 0)
      {
        saveCityCodetoEEP(&CityC);
        readCityCodefromEEP(&CityC);
        
        cityCode = CityCODE;
        
        if(cityCode == "0")
        {
          Serial.println("城市代码调整为：自动");
          getCityCode();  //获取城市代码
        }
        else
        {
          Serial.printf("城市代码调整为：");
          Serial.println(cityCode);
        }
        Serial.println("");
        getCityWeater();//更新城市天气  
        SMOD = "";
      }
      else
        Serial.println("城市调整错误，请输入9位城市代码，自动获取请输入0");
    }   
    if(SMOD=="0x03")//设置3屏幕显示方向
    {
      int RoSet = atoi(incomingByte.c_str());
      if(RoSet >= 0 && RoSet <= 3)
      {
        EEPROM.write(Ro_addr, RoSet);//屏幕方向地址写入方向值
        EEPROM.commit();//保存更改的数据
        SMOD = "";
      //设置屏幕方向后重新刷屏并显示
        // tft.setRotation(RoSet);
        // tft.fillScreen(0x0000);
        LCD_reflash(1);//屏幕刷新程序
        UpdateWeater_en = 1;
        // TJpgDec.drawJpg(15,183,temperature, sizeof(temperature));  //温度图标
        // TJpgDec.drawJpg(15,213,humidity, sizeof(humidity));  //湿度图标

        Serial.print("屏幕方向设置为：");
        Serial.println(RoSet);
      }
      else 
      {
        Serial.println("屏幕方向值错误，请输入0-3内的值");
      }
    }
    if(SMOD=="0x04")//设置天气更新时间
    {
      int wtup = atoi(incomingByte.c_str());//int n = atoi(xxx.c_str());//String转int
      if(wtup>=1 && wtup<=60)
      {
        EEPROM.write(UpWeT_addr, wtup);//亮度地址写入亮度值
        EEPROM.commit();//保存更改的数据
        delay(5);
        updateweater_time = wtup;
        SMOD = "";
        Serial.printf("天气更新时间更改为：");
        Serial.print(updateweater_time);
        Serial.println("分钟");
      }
      else
        Serial.println("更新时间太长，请重新设置（1-60）");
    } 
    else
    {
      SMOD = incomingByte;
      delay(2);
      if(SMOD=="0x01")
        Serial.println("请输入亮度值，范围0-100");
      else if(SMOD=="0x02")
        Serial.println("请输入9位城市代码，自动获取请输入0"); 
      else if(SMOD=="0x03")
      {
        Serial.println("请输入屏幕方向值，"); 
        Serial.println("0-USB接口朝下");
        Serial.println("1-USB接口朝右");
        Serial.println("2-USB接口朝上");
        Serial.println("3-USB接口朝左");
      }
      else if(SMOD=="0x04")
      {
        Serial.print("当前天气更新时间："); 
        Serial.print(updateweater_time);
        Serial.println("分钟");
        Serial.println("请输入天气更新时间（1-60）分钟"); 
      }
      else if(SMOD=="0x05")
      {
        Serial.println("重置WiFi设置中......");
        delay(10);
        wm.resetSettings();
        deletewificonfig();
        delay(10);
        Serial.println("重置WiFi成功");
        SMOD = "";
        ESP.restart();
      }
      else
      {
        Serial.println("");
        Serial.println("请输入需要修改的代码：");
        Serial.println("亮度设置输入        0x01");
        Serial.println("地址设置输入        0x02");
        Serial.println("屏幕方向设置输入    0x03");
        Serial.println("更改天气更新时间    0x04");
        Serial.println("重置WiFi(会重启)    0x05");
        Serial.println("");
      }
    }
  }
}


#if WebSever_EN
//web网站相关函数
// todo web设置页面 - 注释掉了屏幕旋转代码

void handleconfig() 
{
  String msg;
  int web_cc,web_setro,web_lcdbl,web_upt,web_dhten;

  if (server.hasArg("web_ccode") || server.hasArg("web_bl") || \
      server.hasArg("web_upwe_t") || server.hasArg("web_DHT11_en") || server.hasArg("web_set_rotation")) 
  {
    web_cc    = server.arg("web_ccode").toInt();
    web_setro = server.arg("web_set_rotation").toInt();
    web_lcdbl = server.arg("web_bl").toInt();
    web_upt   = server.arg("web_upwe_t").toInt();
    web_dhten = server.arg("web_DHT11_en").toInt();
    Serial.println("");
    if(web_cc>=101000000 && web_cc<=102000000) 
    {
      saveCityCodetoEEP(&web_cc);
      readCityCodefromEEP(&web_cc);
      cityCode = web_cc;
      Serial.print("城市代码:");
      Serial.println(web_cc);
    }
    if(web_cc==0)
    {
      saveCityCodetoEEP(&web_cc);
      readCityCodefromEEP(&web_cc);
      cityCode = web_cc;
      Serial.print("城市代码:AUTO");
    }
    if(web_lcdbl>0 && web_lcdbl<=100)
    {
      EEPROM.write(BL_addr, web_lcdbl);//亮度地址写入亮度值
      EEPROM.commit();//保存更改的数据
      delay(5);
      LCD_BL_PWM = EEPROM.read(BL_addr); 
      delay(5);
      Serial.printf("亮度调整为：");
      u8g2.setContrast(LCD_BL_PWM*2.55);
      Serial.println(LCD_BL_PWM);
      Serial.println("");
    }
    if(web_upt > 0 && web_upt <= 60)
    {
      EEPROM.write(UpWeT_addr, web_upt);//天气更新地址写入天气更新时间
      EEPROM.commit();//保存更改的数据
      delay(5);
      updateweater_time = web_upt;
      Serial.print("天气更新时间（分钟）:");
      Serial.println(web_upt);
    }

    EEPROM.write(DHT_addr, web_dhten);
    EEPROM.commit();//保存更改的数据
    delay(5);
    if(web_dhten != DHT_img_flag)
    {
      DHT_img_flag = web_dhten;
      // tft.fillScreen(0x0000);
      LCD_reflash(1);//屏幕刷新程序
      UpdateWeater_en = 1;
      // TJpgDec.drawJpg(15,183,temperature, sizeof(temperature));  //温度图标
      // TJpgDec.drawJpg(15,213,humidity, sizeof(humidity));  //湿度图标
    }
    Serial.print("DHT Sensor Enable： ");
    Serial.println(DHT_img_flag);

    
    EEPROM.write(Ro_addr, web_setro);
    EEPROM.commit();//保存更改的数据
    delay(5);
    if(web_setro != LCD_Rotation)
    {
      LCD_Rotation = web_setro;
      // tft.setRotation(LCD_Rotation);
      // tft.fillScreen(0x0000);
      LCD_reflash(1);//屏幕刷新程序
      UpdateWeater_en = 1;
      // TJpgDec.drawJpg(15,183,temperature, sizeof(temperature));  //温度图标
      // TJpgDec.drawJpg(15,213,humidity, sizeof(humidity));  //湿度图标
    }
    Serial.print("LCD Rotation:");
    Serial.println(LCD_Rotation);
  }

  //网页界面代码段
  String content = "<!DOCTYPE html><html lang=\"zh-CN\"><meta http-equiv='Content-Type' content='text/html; charset=utf-8' /><title>TDI Console</title><style>html,body{ background: #2F3840; color: #fff; font-size: large; text-align:center;width=100%;height=100%}</style>";
        content += "<body><form action='/' method='POST'><br><h2>TDI 网络控制台</h2><br>";
        content += "<h3>城市代码，输入0为自动:</h3><br><input type='text' name='web_ccode' placeholder='当前为"+String(EEPROM.read(CC_addr))+"'><br>";
        content += "<br><h3>屏幕背光亮度(1-100):(默认:50%)</h3><br><input type='text' name='web_bl' placeholder='当前为"+String(EEPROM.read(BL_addr))+"%'><br>";
        content += "<br><h3>天气刷新时间:(默认10min)</h3><br><input type='text' name='web_upwe_t' placeholder='当前为"+String(EEPROM.read(UpWeT_addr))+"min'><br>";
        #if DHT_EN
        content += "<br>DHT Sensor Enable  <input type='radio' name='web_DHT11_en' value='0'checked> DIS \
                                          <input type='radio' name='web_DHT11_en' value='1'> EN<br>";
        #endif
        content += "<br><h3>屏幕旋转方向</h3><br>\
                    <input type='radio' name='web_set_rotation' value='0' checked> USB 向下<br>\
                    <input type='radio' name='web_set_rotation' value='1'> USB 向右<br>\
                    <input type='radio' name='web_set_rotation' value='2'> USB 向上<br>\
                    <input type='radio' name='web_set_rotation' value='3'> USB 向左<br>";
        content += "<br><div><input type='submit' name='保存设置' value='Save'></form></div>" + msg + "<br>";
        content += "TDI Console by WCY. Modified By JimHan<br>";
        content += "</body></html>";
  server.send(200, "text/html", content);
}


//no need authentication
void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}


//Web服务初始化
void Web_Sever_Init()
{
  uint32_t counttime = 0;//记录创建mDNS的时间
  Serial.println("mDNS responder building...");
  counttime = millis();
  while (!MDNS.begin("SD3"))
  {
    if(millis() - counttime > 30000) ESP.restart();//判断超过30秒钟就重启设备
  }

  Serial.println("mDNS responder started");
  //输出连接wifi后的IP地址
  // Serial.print("本地IP： ");
  // Serial.println(WiFi.localIP());

  server.on("/", handleconfig);
  server.onNotFound(handleNotFound);

  //开启TCP服务
  server.begin();
  Serial.println("HTTP服务器已开启");

  Serial.println("连接: http://sd3.local");
  Serial.print("本地IP： ");
  Serial.println(WiFi.localIP());
  //将服务器添加到mDNS
  MDNS.addService("http", "tcp", 80);
}


//Web网页设置函数
void Web_Sever()
{
  MDNS.update();
  server.handleClient();
}


// todo web服务打开后LCD显示登陆网址及IP- 已修改待验证
void Web_sever_Win()
{
  IPAddress IP_adr = WiFi.localIP();
  // strcpy(IP_adr,WiFi.localIP().toString());

  u8g2.setFont(u8g2_font_missingplanet_tr);
  u8g2.drawUTF8(20,20, "IP:"); 
  u8g2.drawUTF8(40,20, WiFi.localIP().toString().c_str()); 
  u8g2.setFont(u8g2_font_DigitalDiscoThin_tu);
  u8g2.drawUTF8(38,40, "CONSOLE"); //显示版本号信息
  u8g2.setFont(u8g2_font_missingplanet_tr);
  u8g2.drawUTF8(27,60, "http://sd3.local"); //显示版本号信息
  u8g2.sendBuffer();          // transfer internal memory to the display

  // clk.setColorDepth(8);
  // clk.createSprite(200, 70);//创建窗口
  // clk.fillSprite(0x0000);   //填充率

  // clk.drawRoundRect(0,0,200,100,5,0xFFFF);       //空心圆角矩形
  // clk.setTextDatum(CC_DATUM);   //设置文本数据
  // clk.setTextColor(TFT_GREEN, 0x0000); 
  // clk.drawString("Connect to Config:",70,10,2);
  // clk.drawString("IP:",45,60,2);
  // clk.setTextColor(TFT_WHITE, 0x0000); 
  // clk.drawString("http://sd3.local",100,40,4);
  // clk.drawString(&IP_adr,125,70,2);
  // clk.pushSprite(20,40);  //窗口位置
    
  // clk.deleteSprite();
}
#endif


// todo WEB配网LCD显示函数 - 已修改待验证
void Web_win()
{
  u8g2.clearBuffer();          // clear the internal memory
  u8g2.setFont(u8g2_font_wqy14_t_gb2312);
  u8g2.drawUTF8(23,16, "网络连接失败"); 
  u8g2.drawUTF8(1,37, "请连接热点进行配置");
  u8g2.setFont(u8g2_font_missingplanet_tr);
  u8g2.drawUTF8(12,60, "SSID: AutoConnectAP"); //显示版本号信息
  u8g2.sendBuffer();          // transfer internal memory to the display

  // clk.setColorDepth(8);
  
  // clk.createSprite(200, 60);//创建窗口
  // clk.fillSprite(0x0000);   //填充率

  // clk.setTextDatum(CC_DATUM);   //设置文本数据
  // clk.setTextColor(TFT_GREEN, 0x0000); 
  // clk.drawString("WiFi Connect Fail!",100,10,2);
  // clk.drawString("SSID:",45,40,2);
  // clk.setTextColor(TFT_WHITE, 0x0000); 
  // clk.drawString("AutoConnectAP",125,40,2);
  // clk.pushSprite(20,50);  //窗口位置
    
  // clk.deleteSprite();
}


// todo WEB配网函数 - 注释掉了屏幕旋转代码
void Webconfig()
{
  WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP  
  
  delay(3000);
  wm.resetSettings(); // wipe settings
  
  // add a custom input field
  int customFieldLength = 40;
  
  // new (&custom_field) WiFiManagerParameter("customfieldid", "Custom Field Label", "Custom Field Value", customFieldLength,"placeholder=\"Custom Field Placeholder\"");
  
  // test custom html input type(checkbox)
  //  new (&custom_field) WiFiManagerParameter("customfieldid", "Custom Field Label", "Custom Field Value", customFieldLength,"placeholder=\"Custom Field Placeholder\" type=\"checkbox\""); // custom html type
  
  // test custom html(radio)
  // const char* custom_radio_str = "<br/><label for='customfieldid'>Custom Field Label</label><input type='radio' name='customfieldid' value='1' checked> One<br><input type='radio' name='customfieldid' value='2'> Two<br><input type='radio' name='customfieldid' value='3'> Three";
  // new (&custom_field) WiFiManagerParameter(custom_radio_str); // custom html input

  const char* set_rotation = "<br/><label for='set_rotation'>Set Rotation</label>\
                              <input type='radio' name='set_rotation' value='0' checked> One<br>\
                              <input type='radio' name='set_rotation' value='1'> Two<br>\
                              <input type='radio' name='set_rotation' value='2'> Three<br>\
                              <input type='radio' name='set_rotation' value='3'> Four<br>";
  WiFiManagerParameter  custom_rot(set_rotation); // custom html input
  WiFiManagerParameter  custom_bl("LCDBL","LCD BackLight(1-100)","10",3);
  #if DHT_EN
  WiFiManagerParameter  custom_DHT11_en("DHT11_en","Enable DHT11 sensor","0",1);
  #endif
  WiFiManagerParameter  custom_weatertime("WeaterUpdateTime","Weather Update Time(Min)","10",3);
  WiFiManagerParameter  custom_cc("CityCode","CityCode","101250101",9);
  WiFiManagerParameter  p_lineBreak_notext("<p></p>");

  // wm.addParameter(&p_lineBreak_notext);
  // wm.addParameter(&custom_field);
  wm.addParameter(&p_lineBreak_notext);
  wm.addParameter(&custom_cc);
  wm.addParameter(&p_lineBreak_notext);
  wm.addParameter(&custom_bl);
  wm.addParameter(&p_lineBreak_notext);
  wm.addParameter(&custom_weatertime);
  wm.addParameter(&p_lineBreak_notext);
  wm.addParameter(&custom_rot);
  #if DHT_EN
  wm.addParameter(&p_lineBreak_notext);
  wm.addParameter(&custom_DHT11_en);
  #endif
  wm.setSaveParamsCallback(saveParamCallback);
  
  // custom menu via array or vector
  // 
  // menu tokens, "wifi","wifinoscan","info","param","close","sep","erase","restart","exit" (sep is seperator) (if param is in menu, params will not show up in wifi page!)
  // const char* menu[] = {"wifi","info","param","sep","restart","exit"}; 
  // wm.setMenu(menu,6);
  std::vector<const char *> menu = {"wifi","restart"};
  wm.setMenu(menu);
  
  // set dark theme
  wm.setClass("invert");
  
  //set static ip
  // wm.setSTAStaticIPConfig(IPAddress(10,0,1,99), IPAddress(10,0,1,1), IPAddress(255,255,255,0)); // set static ip,gw,sn
  // wm.setShowStaticFields(true); // force show static ip fields
  // wm.setShowDnsFields(true);    // force show dns field always

  // wm.setConnectTimeout(20); // how long to try to connect for before continuing
//  wm.setConfigPortalTimeout(30); // auto close configportal after n seconds
  // wm.setCaptivePortalEnable(false); // disable captive portal redirection
  // wm.setAPClientCheck(true); // avoid timeout if client connected to softap

  // wifi scan settings
  // wm.setRemoveDuplicateAPs(false); // do not remove duplicate ap names (true)
  wm.setMinimumSignalQuality(20);  // set min RSSI (percentage) to show in scans, null = 8%
  // wm.setShowInfoErase(false);      // do not show erase button on info page
  // wm.setScanDispPerc(true);       // show RSSI as percentage not graph icons
  
  // wm.setBreakAfterConfig(true);   // always exit configportal even if wifi save fails

  bool res;
  // res = wm.autoConnect(); // auto generated AP name from chipid
   res = wm.autoConnect("AutoConnectAP"); // anonymous ap
  //  res = wm.autoConnect("AutoConnectAP","password"); // password protected ap
  
  while(!res);
}


String getParam(String name){
  //read parameter from server, for customhmtl input
  String value;
  if(wm.server->hasArg(name)) {
    value = wm.server->arg(name);
  }
  return value;
}


void saveParamCallback(){
  int cc;
  
  Serial.println("[CALLBACK] saveParamCallback fired");
  // Serial.println("PARAM customfieldid = " + getParam("customfieldid"));
  // Serial.println("PARAM CityCode = " + getParam("CityCode"));
  // Serial.println("PARAM LCD BackLight = " + getParam("LCDBL"));
  // Serial.println("PARAM WeaterUpdateTime = " + getParam("WeaterUpdateTime"));
  // Serial.println("PARAM Rotation = " + getParam("set_rotation"));
  // Serial.println("PARAM DHT11_en = " + getParam("DHT11_en"));
  
  //将从页面中获取的数据保存
  #if DHT_EN
  DHT_img_flag = getParam("DHT11_en").toInt();
  #endif
  updateweater_time = getParam("WeaterUpdateTime").toInt();
  cc =  getParam("CityCode").toInt();
  LCD_Rotation = getParam("set_rotation").toInt();
  LCD_BL_PWM = getParam("LCDBL").toInt();

  //对获取的数据进行处理
  //城市代码
  Serial.print("CityCode = ");
  Serial.println(cc);
  if(cc>=101000000 && cc<=102000000 || cc == 0)
  {
    saveCityCodetoEEP(&cc); //城市地址写入城市代码
    readCityCodefromEEP(&cc); //从EEPROM读取城市代码
    cityCode = cc;
  }
  //屏幕方向
  Serial.print("LCD_Rotation = ");
  Serial.println(LCD_Rotation);
  if(EEPROM.read(Ro_addr) != LCD_Rotation)
  {
    EEPROM.write(Ro_addr, LCD_Rotation);
    EEPROM.commit();
    delay(5);
  }
  // tft.setRotation(LCD_Rotation);
  // tft.fillScreen(0x0000);
  Web_win();
  loadNum--;
  loading(1);
  if(EEPROM.read(BL_addr) != LCD_BL_PWM)
  {
    EEPROM.write(BL_addr, LCD_BL_PWM);
    EEPROM.commit();
    delay(5);
  }
  //屏幕亮度
  Serial.printf("亮度调整为：");
  u8g2.setContrast(LCD_BL_PWM*2.55);
  Serial.println(LCD_BL_PWM);
  //天气更新时间
  Serial.printf("天气更新时间调整为：");
  Serial.println(updateweater_time);

  #if DHT_EN
  //是否使用DHT11传感器
  Serial.printf("DHT11传感器：");
  EEPROM.write(DHT_addr, DHT_img_flag);
  EEPROM.commit();//保存更改的数据
  Serial.println((DHT_img_flag?"已启用":"未启用"));
  #endif
}


void setup()
{
  Serial.begin(115200);
  EEPROM.begin(1024);
  //WiFi.forceSleepWake();
  //wm.resetSettings();    //在初始化中使wifi重置，需重新配置WiFi
  
  #if DHT_EN
  dht.begin();
  //从eeprom读取DHT传感器使能标志
  DHT_img_flag = EEPROM.read(DHT_addr);
  #endif
  
  u8g2.begin(); // 启动OLED
  u8g2.clearBuffer();          // clear the internal memory
  u8g2.setFont(u8g2_font_ncenB08_tr); // choose a suitable font
  
  //从eeprom读取背光亮度设置
  if(EEPROM.read(BL_addr)>0&&EEPROM.read(BL_addr)<100)
    LCD_BL_PWM = EEPROM.read(BL_addr); 
  u8g2.setContrast(LCD_BL_PWM);
  
  //从eeprom读取天气更新时间
  updateweater_time = EEPROM.read(UpWeT_addr);

  targetTime = millis() + 1000; 
  readwificonfig();//读取存储的wifi信息
  Serial.print("正在连接WIFI ");
  Serial.println(wificonf.stassid);
  WiFi.begin(wificonf.stassid, wificonf.stapsw);
  
  // TJpgDec.setJpgScale(1);
  // TJpgDec.setSwapBytes(true);
  // TJpgDec.setCallback(tft_output); 注释掉LCD图片库

  while (WiFi.status() != WL_CONNECTED) 
  {
    loading(10);  
      
    if(loadNum>=104)
    {
      //使能web配网
      Web_win();
      Webconfig();
      u8g2.clearBuffer();          // clear the internal memory
      break;
    }
  }
  delay(10); 
  while(loadNum < 104) //让动画走完
  { 
    loading(1);
  }
  u8g2.clearBuffer();          // clear the internal memory

  if(WiFi.status() == WL_CONNECTED)
  {
    // Serial.print("SSID:");
    // Serial.println(WiFi.SSID().c_str());
    // Serial.print("PSW:");
    // Serial.println(WiFi.psk().c_str());
    strcpy(wificonf.stassid,WiFi.SSID().c_str());//名称复制
    strcpy(wificonf.stapsw,WiFi.psk().c_str());//密码复制
    savewificonfig();
    readwificonfig();
    #if WebSever_EN
    //开启web服务器初始化
    Web_Sever_Init();
    Web_sever_Win();
    delay(10000);
    #endif
  }

  // Serial.print("本地IP： ");
  // Serial.println(WiFi.localIP());
  Serial.println("启动UDP");
  Udp.begin(localPort);
  Serial.println("等待同步...");
  setSyncProvider(getNtpTime);
  setSyncInterval(400);

  // TJpgDec.setJpgScale(1);
  // TJpgDec.setSwapBytes(true);
  // TJpgDec.setCallback(tft_output); 注释掉LCD图片库
  
  int CityCODE = 0;
  readCityCodefromEEP(&CityCODE); //从EEPROM读取城市代码
  if(CityCODE>=101000000 && CityCODE<=102000000) 
    cityCode = CityCODE;  
  else
    getCityCode();  //获取城市代码
  
  u8g2.clearDisplay();//清屏
  getCityWeater();
#if DHT_EN
  if(DHT_img_flag != 0)
  IndoorTem();
#endif
#if !WebSever_EN
  WiFi.forceSleepBegin(); //wifi off
  Serial.println("WIFI休眠......");
  Wifi_en = 0;
#endif
}


void loop()
{
  #if WebSever_EN
  Web_Sever();
  #endif
  LCD_reflash(0);
  u8g2.sendBuffer();          // transfer internal memory to the display
  Serial_set();
}


void LCD_reflash(int en) //全屏刷新时钟信息
{
  if (now() != prevDisplay || en == 1) 
  {
    prevDisplay = now();
    digitalClockDisplay(en);
    prevTime=0;  
  }
  
  //三秒钟更新一次
  if(second()%3 ==0&& prevTime == 0 || en == 1){
#if DHT_EN
    if(DHT_img_flag != 0)
    IndoorTem();
#endif
    scrollBanner();
  }


  if(millis() - weaterTime > (60000*updateweater_time) || en == 1 || UpdateWeater_en != 0){ //10分钟更新一次天气
    if(Wifi_en == 0)
    {
      WiFi.forceSleepWake();//wifi on
      Serial.println("WIFI恢复......");
      Wifi_en = 1;
    }

    if(WiFi.status() == WL_CONNECTED)
    {
      Serial.println("WIFI已连接");
      getCityWeater();
      if(UpdateWeater_en != 0) UpdateWeater_en = 0;
      weaterTime = millis();
      // while(!getNtpTime());
      getNtpTime();
      #if !WebSever_EN
      WiFi.forceSleepBegin(); // Wifi Off
      Serial.println("WIFI休眠......");
      Wifi_en = 0;
      #endif
    }
  }
}


// 发送HTTP请求并且将服务器响应通过串口输出
void getCityCode(){
 String URL = "http://wgeo.weather.com.cn/ip/?_="+String(now());
  //创建 HTTPClient 对象
  HTTPClient httpClient;
 
  //配置请求地址。此处也可以不使用端口号和PATH而单纯的
  httpClient.begin(wificlient,URL); 
  
  //设置请求头中的User-Agent
  httpClient.setUserAgent("Mozilla/5.0 (iPhone; CPU iPhone OS 11_0 like Mac OS X) AppleWebKit/604.1.38 (KHTML, like Gecko) Version/11.0 Mobile/15A372 Safari/604.1");
  httpClient.addHeader("Referer", "http://www.weather.com.cn/");
 
  //启动连接并发送HTTP请求
  int httpCode = httpClient.GET();
  Serial.print("Send GET request to URL: ");
  Serial.println(URL);
  
  //如果服务器响应OK则从服务器获取响应体信息并通过串口输出
  if (httpCode == HTTP_CODE_OK) {
    String str = httpClient.getString();
    
    int aa = str.indexOf("id=");
    if(aa>-1)
    {
       //cityCode = str.substring(aa+4,aa+4+9).toInt();
       cityCode = str.substring(aa+4,aa+4+9);
       Serial.println(cityCode); 
       getCityWeater();
    }
    else
    {
      Serial.println("获取城市代码失败");  
    }
    
    
  } else {
    Serial.println("请求城市代码错误：");
    Serial.println(httpCode);
  }
 
  //关闭ESP8266与服务器连接
  httpClient.end();
}


// 获取城市天气
void getCityWeater(){
 //String URL = "http://d1.weather.com.cn/dingzhi/" + cityCode + ".html?_="+String(now());//新
 String URL = "http://d1.weather.com.cn/weather_index/" + String(cityCode) + ".html?_="+String(now());//原来
  //创建 HTTPClient 对象
  HTTPClient httpClient;
  
  httpClient.begin(wificlient,URL); 
  
  //设置请求头中的User-Agent
  httpClient.setUserAgent("Mozilla/5.0 (iPhone; CPU iPhone OS 11_0 like Mac OS X) AppleWebKit/604.1.38 (KHTML, like Gecko) Version/11.0 Mobile/15A372 Safari/604.1");
  httpClient.addHeader("Referer", "http://www.weather.com.cn/");
 
  //启动连接并发送HTTP请求
  int httpCode = httpClient.GET();
  Serial.println("正在获取天气数据");
  Serial.println(URL);
  
  //如果服务器响应OK则从服务器获取响应体信息并通过串口输出
  if (httpCode == HTTP_CODE_OK) {

    String str = httpClient.getString();
    int indexStart = str.indexOf("weatherinfo\":");
    int indexEnd = str.indexOf("};var alarmDZ");

    String jsonCityDZ = str.substring(indexStart+13,indexEnd);
    //Serial.println(jsonCityDZ);

    indexStart = str.indexOf("dataSK =");
    indexEnd = str.indexOf(";var dataZS");
    String jsonDataSK = str.substring(indexStart+8,indexEnd);
    //Serial.println(jsonDataSK);

    
    indexStart = str.indexOf("\"f\":[");
    indexEnd = str.indexOf(",{\"fa");
    String jsonFC = str.substring(indexStart+5,indexEnd);
    //Serial.println(jsonFC);
    
    weaterData(&jsonCityDZ,&jsonDataSK,&jsonFC);
    Serial.println("获取成功");
    Serial.print(jsonDataSK);
    
  } else {
    Serial.println("请求城市天气错误：");
    Serial.print(httpCode);
  }
 
  //关闭ESP8266与服务器连接
  httpClient.end();
}


String scrollText[7];//天气信息存储

//天气信息写到屏幕上
void weaterData(String *cityDZ,String *dataSK,String *dataFC)
{
  //解析第一段JSON
  DynamicJsonDocument doc(1024);
  deserializeJson(doc, *dataSK);
  JsonObject sk = doc.as<JsonObject>();

  //TFT_eSprite clkb = TFT_eSprite(&tft);
  
  /***绘制相关文字***/
  // clk.setColorDepth(8);
  // clk.loadFont(ZdyLwFont_20);
  
  //温度
  u8g2.setFont(u8g2_font_wqy13_t_gb2312);
  String Temp_Combine = (sk["temp"].as<String>())+"℃";
  u8g2.drawUTF8(35,63, Temp_Combine.c_str()); 
  u8g2.drawUTF8(5,63, "温度"); 
  u8g2.setFont(u8g2_font_wqy16_t_chinese1);

  // clk.createSprite(58, 24); 
  // clk.fillSprite(bgColor);
  // clk.setTextDatum(CC_DATUM);
  // clk.setTextColor(TFT_WHITE, bgColor); 
  // clk.drawString(sk["temp"].as<String>()+"℃",28,13);
  // clk.pushSprite(100,184);
  // clk.deleteSprite();
  tempnum = sk["temp"].as<int>();
  tempnum = tempnum+10;
//  if(tempnum<10)
//    tempcol=0x00FF;
//  else if(tempnum<28)
//    tempcol=0x0AFF;
//  else if(tempnum<34)
//    tempcol=0x0F0F;
//  else if(tempnum<41)
//    tempcol=0xFF0F;
//  else if(tempnum<49)
//    tempcol=0xF00F;
//  else
//  {
//    tempcol=0xF00F;
//    tempnum=50;
//  }
  //tempWin();
  
  //湿度
  u8g2.setFont(u8g2_font_wqy13_t_gb2312);
  u8g2.drawUTF8(95,63, sk["SD"].as<String>().c_str()); 
  u8g2.drawUTF8(65,63, "湿度"); 
  // clk.createSprite(58, 24); 
  // clk.fillSprite(bgColor);
  // clk.setTextDatum(CC_DATUM);
  // clk.setTextColor(TFT_WHITE, bgColor); 
  // clk.drawString(sk["SD"].as<String>(),28,13);
  //clk.drawString("100%",28,13);
  // clk.pushSprite(100,214);
  // clk.deleteSprite();
  //String A = sk["SD"].as<String>();
  huminum = atoi((sk["SD"].as<String>()).substring(0,2).c_str());
  
//  if(huminum>90)
//    humicol=0x00FF;
//  else if(huminum>70)
//    humicol=0x0AFF;
//  else if(huminum>40)
//    humicol=0x0F0F;
//  else if(huminum>20)
//    humicol=0xFF0F;
//  else
//    humicol=0xF00F;
  //humidityWin();

  
  //城市名称
  u8g2.setFont(u8g2_font_wqy13_t_gb2312);
  u8g2.drawUTF8(5,14, sk["cityname"].as<String>().c_str()); 
  // clk.createSprite(94, 30); 
  // clk.fillSprite(bgColor);
  // clk.setTextDatum(CC_DATUM);
  // clk.setTextColor(TFT_WHITE, bgColor); 
  // clk.drawString(sk["cityname"].as<String>(),44,16);
  // clk.pushSprite(15,15);
  // clk.deleteSprite();

  //PM2.5空气指数-删除了所有颜色块
  // uint16_t pm25BgColor = tft.color565(156,202,127);//优
  String aqiTxt = "优";
  int pm25V = sk["aqi"];
  if(pm25V>200){
    // pm25BgColor = tft.color565(136,11,32);//重度
    aqiTxt = "重度";
  }else if(pm25V>150){
    // pm25BgColor = tft.color565(186,55,121);//中度
    aqiTxt = "中度";
  }else if(pm25V>100){
    // pm25BgColor = tft.color565(242,159,57);//轻
    aqiTxt = "轻度";
  }else if(pm25V>50){
    // pm25BgColor = tft.color565(247,219,100);//良
    aqiTxt = "良";
  }
  u8g2.setFont(u8g2_font_wqy13_t_gb2312);
  //u8g2.drawUTF8(95,43, aqiTxt.c_str());  删除多余的城市空气质量单独显示 
  // clk.createSprite(56, 24); 
  // clk.fillSprite(bgColor);
  // clk.fillRoundRect(0,0,50,24,4,pm25BgColor);
  // clk.setTextDatum(CC_DATUM);
  // clk.setTextColor(0x0000); 
  // clk.drawString(aqiTxt,25,13);
  // clk.pushSprite(104,18);
  // clk.deleteSprite();
  
  scrollText[0] = "实时天气 "+sk["weather"].as<String>();
  scrollText[1] = "空气质量 "+aqiTxt;
  scrollText[2] = "风向 "+sk["WD"].as<String>()+sk["WS"].as<String>();

  //scrollText[6] = atoi((sk["weathercode"].as<String>()).substring(1,3).c_str()) ;

  //天气图标
  //wrat.printfweather(170,15,atoi((sk["weathercode"].as<String>()).substring(1,3).c_str()));

  
  //左上角滚动字幕
  //解析第二段JSON
  deserializeJson(doc, *cityDZ);
  JsonObject dz = doc.as<JsonObject>();
  //Serial.println(sk["ws"].as<String>());
  //横向滚动方式
  //String aa = "今日天气:" + dz["weather"].as<String>() + "，温度:最低" + dz["tempn"].as<String>() + "，最高" + dz["temp"].as<String>() + " 空气质量:" + aqiTxt + "，风向:" + dz["wd"].as<String>() + dz["ws"].as<String>();
  //scrollTextWidth = clk.textWidth(scrollText);
  //Serial.println(aa);
  scrollText[3] = "今日"+dz["weather"].as<String>();
  
  deserializeJson(doc, *dataFC);
  JsonObject fc = doc.as<JsonObject>();
  
  scrollText[4] = "最低温度"+fc["fd"].as<String>()+"℃";
  scrollText[5] = "最高温度"+fc["fc"].as<String>()+"℃";
  
  //Serial.println(scrollText[0]);
  
  // clk.unloadFont();
}

int currentIndex = 0;
// TFT_eSprite clkb = TFT_eSprite(&tft);

void scrollBanner(){
  //if(millis() - prevTime > 2333) //3秒切换一次
//  if(second()%2 ==0&& prevTime == 0)
//  { 
    if(scrollText[currentIndex])
    {
      u8g2.setFont(u8g2_font_wqy13_t_gb2312);
      u8g2.setDrawColor(0);   // clear the scrolling area
      u8g2.drawBox(35, 0, u8g2.getDisplayWidth()-35, 15);
      u8g2.setDrawColor(1);   // set the color for the text
      u8g2.drawUTF8(35,14,scrollText[currentIndex].c_str()); 
      // clkb.setColorDepth(8);
      // clkb.loadFont(ZdyLwFont_20);
      // clkb.createSprite(150, 30); 
      // clkb.fillSprite(bgColor);
      // clkb.setTextWrap(false);
      // clkb.setTextDatum(CC_DATUM);
      // clkb.setTextColor(TFT_WHITE, bgColor); 
      // clkb.drawString(scrollText[currentIndex],74, 16);
      // clkb.pushSprite(10,45);
      
      // clkb.deleteSprite();
      // clkb.unloadFont();
      
      if(currentIndex>=5)
        currentIndex = 0;  //回第一个
      else
        currentIndex += 1;  //准备切换到下一个        
    }
    prevTime = 1;
//  }
}


unsigned char Hour_sign   = 60;
unsigned char Minute_sign = 60;
unsigned char Second_sign = 60;
void digitalClockDisplay(int reflash_en)
{ 
  int timey=82;
  if(hour()!=Hour_sign || reflash_en == 1)//时钟刷新
  {
    u8g2.setFont(u8g2_font_freedoomr25_tn);
    u8g2.setDrawColor(0);   // clear the scrolling area
    u8g2.drawBox(5, 21, 40, 28);
    u8g2.setDrawColor(1);   // set the color for the text
    u8g2.drawStr(5,48,String(hour()/10).c_str());
    u8g2.drawStr(25,48,String(hour()%10).c_str());
    // dig.printfW3660(20,timey,hour()/10);
    // dig.printfW3660(60,timey,hour()%10);
    Hour_sign = hour();
  }
  if(minute()!=Minute_sign  || reflash_en == 1)//分钟刷新
  {
    u8g2.setFont(u8g2_font_freedoomr25_tn);
    u8g2.setDrawColor(0);   // clear the scrolling area
    u8g2.drawBox(50, 21, 40, 28);
    u8g2.setDrawColor(1);   // set the color for the text
    u8g2.drawStr(50,48,String(minute()/10).c_str());
    u8g2.drawStr(70,48,String(minute()%10).c_str());
    // dig.printfO3660(101,timey,minute()/10);
    // dig.printfO3660(141,timey,minute()%10);
    Minute_sign = minute();
  }
  if(second()!=Second_sign  || reflash_en == 1)//秒钟刷新
  {
    if(second()%2==1)
    {u8g2.setDrawColor(1);   // set the color for the sec
    u8g2.drawBox(46, 24, 2, 7);u8g2.drawBox(46, 37, 2, 7);}
    else
    {u8g2.setDrawColor(0);   // clear the sec
    u8g2.drawBox(46, 21, 2, 28);u8g2.setDrawColor(1);}
    // u8g2.setFont(u8g2_font_VCR_OSD_tu);
    // u8g2.drawStr(90,40,String(second()).c_str()); 不再显示数字秒
    // dig.printfW1830(182,timey+30,second()/10);
    // dig.printfW1830(202,timey+30,second()%10);
    Second_sign = second();
  }
  
  if(reflash_en == 1) reflash_en = 0;
  /***日期****/
  u8g2.setFont(u8g2_font_wqy13_t_gb2312);
  u8g2.drawUTF8(95,45,String(week()).c_str());
  u8g2.drawUTF8(95,30,String(monthDay()).c_str());
  // clk.setColorDepth(8);
  // clk.loadFont(ZdyLwFont_20);
  
  //星期
  // clk.createSprite(58, 30);
  // clk.fillSprite(bgColor);
  // clk.setTextDatum(CC_DATUM);
  // clk.setTextColor(TFT_WHITE, bgColor);
  // clk.drawString(week(),29,16);
  // clk.pushSprite(102,150);
  // clk.deleteSprite();
  
  //月日
  // clk.createSprite(95, 30);
  // clk.fillSprite(bgColor);
  // clk.setTextDatum(CC_DATUM);
  // clk.setTextColor(TFT_WHITE, bgColor);  
  // clk.drawString(monthDay(),49,16);
  // clk.pushSprite(5,150);
  // clk.deleteSprite();
  
  // clk.unloadFont();
  /***日期****/
}

//星期
String week()
{
  String wk[7] = {"日","一","二","三","四","五","六"};
  String s = "周" + wk[weekday()-1];
  return s;
}

//月日
String monthDay()
{
  String s = String(month());
  s = s + "/" + day() + "";
  return s;
}

/*-------- NTP code ----------*/

const int NTP_PACKET_SIZE = 48; // NTP时间在消息的前48字节中
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets

time_t getNtpTime()
{
  IPAddress ntpServerIP; // NTP server's ip address

  while (Udp.parsePacket() > 0) ; // discard any previously received packets
  //Serial.println("Transmit NTP Request");
  // get a random server from the pool
  WiFi.hostByName(ntpServerName, ntpServerIP);
  //Serial.print(ntpServerName);
  //Serial.print(": ");
  //Serial.println(ntpServerIP);
  sendNTPpacket(ntpServerIP);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      Serial.println("Receive NTP Response");
      Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      //Serial.println(secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR);
      return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
    }
  }
  Serial.println("No NTP Response :-(");
  return 0; // 无法获取时间时返回0
}

// 向NTP服务器发送请求
void sendNTPpacket(IPAddress &address)
{
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}
