<p align="center">
<img src="./TDI-Show.png" height=200px></p>
<h1 align="center"> Tiny Desktop Indicator </h1>

<p align="center">
<img src="https://img.shields.io/badge/build-passing-green.svg?style=flat-square">
<img src="https://img.shields.io/badge/Version-1.2.1 Stable-red.svg?style=flat-square">
<img src="https://img.shields.io/badge/Language-C++-pink.svg?style=flat-square">
<img src="https://img.shields.io/badge/Developer-JimHan-blue.svg?style=flat-square">
</p>

## An AIot Smart Clock & Weather Forecaster based on Arduino IDE and ESP8266
## 💻基于SDD 项目修改，使用Arduino IDE开发，适用于ESP8266的智能桌面时钟显示器 OLED版本，使用基于SSD1306显示驱动芯片的0.96英寸单色OLED IIC总线屏幕作为显示单元。
---
### 📚 功能 / Functions
- WEB自动配网，记住网络环境
- 当前天气显示，支持自动根据IP获取天气
- 滚动天气详情显示
- 冒号秒针显示，优雅自然
- 使用u8g2 控制OLED IIC屏幕，接线简单代码简洁，功耗低
- 其实就是所有SDD都支持的功能换成OLED屏幕显示...
- 更多功能开发中...
---
### 📤 更新
- 22-04-22 更新v1.2.1版本 删除了所有TFT-spi依赖项目，加上时间冒号闪动，天气修改为3秒切换，优化web控制台显示，现在可以显示当前设定值。
- 22-03-01 更新v1.1.2版本 更新了时间字体，优化了排版，更新网页配置面板为SDI新版外观。解决了时间字体重叠问题。
- 22-03-01 更新v1.0.1版本 删除了所有TFT-spi依赖项目，加上时间冒号闪动，天气修改为3秒切换，优化web控制台显示，现在可以显示当前设定值。
---
### 🔧 待解决的问题
- [x] 使用u8g2加入更多类型屏幕的自适应UI支持
- [ ] 优化显示性能
- [x] 未完全剥离TFT-eSPI库服务