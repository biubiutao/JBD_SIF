// 08 61 02 00 02 80 40 02 68 80 10 00 00 00 53                     //数据格式
// |  |   |  |  |  |  |   |   |   |          |
// 帧 型  停 故 挡 云  灯  速  电  错          校
// 头 号  车 障 位 动  条  度  量  码          验

#include <BLEDevice.h>
#define RX_PIN  0                                                   //接收引脚
#define TX_PIN  1                                                   //发送引脚

static BLEAddress MAC("xx:xx:xx:xx:xx:xx");                         //设备地址
static bool connected = false;
static bool doConnect = false;
static rmt_data_t rmt_recv[72];
static rmt_data_t rmt_send[128];
static uint8_t sendData[15] = {0};
static uint8_t cmd[7] = {0xDD,0xA5,0x03,0x00,0xFF,0xFD,0x77};
static uint8_t soc = 0;

BLEClient* pClient = nullptr;
BLEAdvertisedDevice* myDevice = nullptr;
BLERemoteCharacteristic* pTXD = nullptr;
BLERemoteCharacteristic* pRXD = nullptr;

class MyClientCallback : public BLEClientCallbacks {                //连接回调
  void onConnect(BLEClient* pclient) { connected = true; }
  void onDisconnect(BLEClient* pclient) { connected = false; }
};

void notifyCallback(BLERemoteCharacteristic*                        //获取电量
     pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) { soc = pData[23]; }

class BLECallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    if (advertisedDevice.isAdvertisingService(BLEUUID("FF00")) && advertisedDevice.getAddress() == MAC) {
      BLEDevice::getScan()->stop();
      myDevice = new BLEAdvertisedDevice(advertisedDevice);         //扫描设备
      doConnect = true;
    }
  }
};

bool conToBMS() {
  pClient = BLEDevice::createClient();
  pClient->setClientCallbacks(new MyClientCallback());
  if (!pClient->connect(myDevice)) return false;                    //建立连接
  BLERemoteService* pRemoteService = pClient->getService("FF00");
  pRXD = pRemoteService->getCharacteristic("FF01");
  pTXD = pRemoteService->getCharacteristic("FF02");
  if (!pTXD || !pRXD) return false;
  if (pRXD->canNotify()) pRXD->registerForNotify(notifyCallback);   //注册通知
  return true;
}

void rmtTask(void* arg) { while (1) {
    size_t rmt_num = 72;
    memset(sendData, 0, 15);
    rmtRead(RX_PIN, rmt_recv, &rmt_num, 250);                       //接收一帧
    for (size_t i = 0; i < rmt_num && i < 72; i++) {
      sendData[i/8] |= rmt_recv[i].duration1 < 767 ? 1 << (7 - i%8) : 0;
    } if (sendData[0] != 0x08 || sendData[1] != 0x61) continue;
   
    uint16_t sped = (sendData[7] << 8 | sendData[8]) * 3 >> 2;      //速度校准
    uint8_t tempData[6] = {0x80, 0x40, sped>>8, sped&0xFF, 0x80+soc, 0x10};
    memcpy(sendData + 5, tempData, 6);
    for (uint8_t i = 0; i < 14; i++) sendData[14] ^= sendData[i];
    
    for (uint8_t i = 0; i < 120; i++) {
      bool bit = sendData[i/8] & 1 << (7 - i%8);
      rmt_send[i+1] = {bit ? 511 : 1023, 0, bit ? 1023 : 511, 1};
    } rmt_send[0] = {511, 0, 1023, 1};
    rmtWrite(TX_PIN, rmt_send, 121, 10);                            //发送新帧
    
    for (uint8_t i = 0; i < 15; i++) {
      if (sendData[i] < 0x10)         Serial.print("0");
      Serial.print(sendData[i], HEX); Serial.print(" ");            //串口打印
    } Serial.println();
  }
}

void setup() {
  Serial.begin(115200);
  BLEDevice::init("");
  BLEDevice::getScan()->setAdvertisedDeviceCallbacks(new BLECallbacks());
  
  rmtInit(RX_PIN, RMT_RX_MODE, RMT_MEM_NUM_BLOCKS_1, 1000000);      //基础配置
  rmtInit(TX_PIN, RMT_TX_MODE, RMT_MEM_NUM_BLOCKS_1, 1000000);
  rmtSetRxMinThreshold(RX_PIN, 2);
  rmtSetRxMaxThreshold(RX_PIN, 2000);
  xTaskCreate(rmtTask, "rmtTask", 2048, NULL, 1, NULL);
}

void loop() {
  if (connected) pTXD->writeValue(cmd, 7);                          //发送指令
  else BLEDevice::getScan()->start(1, false);
  if (doConnect) {
    if (conToBMS()) connected = true;
    doConnect = false;
  }
  vTaskDelay(5000);
}
