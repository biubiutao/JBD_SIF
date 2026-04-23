# JBD_SIF
连接嘉佰达BMS获取soc，显示到台铃原车仪表

基于ESP32-C3，截获车上控制器的通讯，插入soc字节，顺便把速度校准为75%，再以SIF发往仪表

需破线改动，断开原本YXT连接至引脚0，引脚1焊线至仪表YXT位置，从仪表PCB连接5V电源

注释掉源码BLEClient.cpp中相应部分参考https://github.com/espressif/arduino-esp32/issues/12222

将MAC改为你自己设备地址，使用Arduino可直接编译下载

尊重作者劳动成果，转载请注明出处

致谢https://github.com/syssi/esphome-jbd-bms/
