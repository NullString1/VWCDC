// Created: 2024-03-01
// Last Modified: 2025-03-16
// Created by: Daniel Kern (NullString1)
// Description: Port of VAG_CDC by shyd for ESP32. Emulates CD changer for vw/audi/skoda/seat head units.
// Related Projects: tomaskovacik/vwcdavr shyd/avr-raspberry-pi-vw-beta-vag-cdc-faker k9spud/vwcdpic
// License: GPL-3.0 license
// Version: 1.1
// Website: https://nullstring.one
// Repository: https://github.com/NullString1/VWCDC

// #define DEBUG
#define CDC_PREFIX1 0x53
#define CDC_PREFIX2 0x2C

#define CDC_END_CMD 0x14
#define CDC_PLAY 0xE4
#define CDC_STOP 0x10
#define CDC_NEXT 0xF8
#define CDC_PREV 0x78
#define CDC_SEEK_FWD 0xD8
#define CDC_SEEK_RWD 0x58
#define CDC_CD1 0x0C
#define CDC_CD2 0x8C
#define CDC_CD3 0x4C
#define CDC_CD4 0xCC
#define CDC_CD5 0x2C
#define CDC_CD6 0xAC
#define CDC_SCAN 0xA0
#define CDC_SFL 0x60
#define CDC_PLAY_NORMAL 0x08

#define MODE_PLAY 0xFF
#define MODE_SHFFL 0x55
#define MODE_SCAN 0x00

#include <Arduino.h>
#include <SPI.h>

volatile long prevMillis = 0;

uint8_t cd;
uint8_t tr;
uint8_t mode;

void send_package(uint8_t c0, uint8_t c1, uint8_t c2, uint8_t c3, uint8_t c4, uint8_t c5, uint8_t c6, uint8_t c7) {
  SPI.beginTransaction(SPISettings(62500, MSBFIRST, SPI_MODE1));
  SPI.transfer(c0);
  delayMicroseconds(874);
  SPI.transfer(c1);
  delayMicroseconds(874);
  SPI.transfer(c2);
  delayMicroseconds(874);
  SPI.transfer(c3);
  delayMicroseconds(874);
  SPI.transfer(c4);
  delayMicroseconds(874);
  SPI.transfer(c5);
  delayMicroseconds(874);
  SPI.transfer(c6);
  delayMicroseconds(874);
  SPI.transfer(c7);
  SPI.endTransaction();
}

void setup() {
  cd = 1;
  tr = 1;
  mode = MODE_PLAY;

  #ifdef DEBUG
  Serial.begin(9600);
  #endif

  delay(1000); // 1000ms wait for radio to boot

  // init SPI
  SPI.begin(SCK, MISO, MOSI, SS);

  send_package(0x74, 0xBE, 0xFE, 0xFF, 0xFF, 0xFF, 0x8F, 0x7C); // idle
  delayMicroseconds(10000);
  send_package(0x34, 0xFF, 0xFE, 0xFE, 0xFE, 0xFF, 0xFA, 0x3C); // load disc
  delayMicroseconds(100000);
  send_package(0x74, 0xBE, 0xFE, 0xFF, 0xFF, 0xFF, 0x8F, 0x7C); // idle
  delayMicroseconds(10000);

  #ifdef DEBUG
  Serial.println("Sent idle/load/idle commands");
  #endif
}

void loop() {
  if ((millis() - prevMillis) > 50) {
    //                  disc      trk         min  sec
    send_package(0x34, 0xBF ^ cd, 0xFF ^ tr, 0xFF, 0xFF, mode, 0xCF, 0x3C);
    prevMillis = millis(); // reset timer
    #ifdef DEBUG
    Serial.println("Sent packet");
    #endif
  }
}