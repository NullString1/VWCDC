#include <Arduino.h>
#include <SPI.h>

// #define DEBUG

#define RADIO_OUT 17
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

volatile uint16_t capTimeHigh = 0;
volatile uint16_t capTimeLow = 0;
volatile uint32_t cmd = 0;

struct flags
{
  union
  {
    unsigned char value;
    struct
    {
      unsigned startCapturing : 1;
      unsigned capturingBytes : 1;
      unsigned newCMD : 1;
      unsigned shutdownPending : 1;
      unsigned bitsCaptured : 8;
    };
  };
} __attribute__((packed));

volatile struct flags f;

uint8_t getCommand(uint32_t cmd);

uint8_t cd;
uint8_t tr;
uint8_t mode;

hw_timer_t *timer1;

void delay_ms(uint32_t us)
{
  delayMicroseconds(us * 1000);
}

void IRAM_ATTR INT0_vect() // remote signals
{
  if (digitalRead(RADIO_OUT)) // if radio out high
  {
    if (f.startCapturing || f.capturingBytes)
    {
      capTimeLow = timerRead(timer1);
    }
    else
      f.startCapturing = 1;
    timerWrite(timer1, 0);

    // eval times
    if (capTimeHigh > 8300 && capTimeLow > 3500)
    {
      f.startCapturing = 0;
      f.capturingBytes = 1;
    }
    else if (f.capturingBytes && capTimeLow > 1500)
    {
      // Bit 1
      cmd = (cmd << 1) | 0x00000001;
      f.bitsCaptured++;
    }
    else if (f.capturingBytes && capTimeLow > 500)
    {
      // Bit 0
      cmd = (cmd << 1);
      f.bitsCaptured++;
    }
    else
    {
      // Nothing
    }
    if (f.bitsCaptured == 32)
    {
      // New command
      f.newCMD = 1;
      f.bitsCaptured = 0;
      f.capturingBytes = 0;
    }
  }
  else
  {
    capTimeHigh = timerRead(timer1);
    timerWrite(timer1, 0);
  }
}

uint8_t spi_xmit(uint8_t val)
{
  // SPI Type: Master
  // SPI Clock Rate: 62,500 kHz
  // SPI Clock Phase: Cycle Start
  // SPI Clock Polarity: Low
  // SPI Data Order: MSB First
  SPI.beginTransaction(SPISettings(62500, MSBFIRST, SPI_MODE1));
  SPI.transfer(val);
  SPI.endTransaction();
  return val;
}

void send_package(uint8_t c0, uint8_t c1, uint8_t c2, uint8_t c3, uint8_t c4, uint8_t c5, uint8_t c6, uint8_t c7)
{
  spi_xmit(c0);
  delayMicroseconds(874);
  spi_xmit(c1);
  delayMicroseconds(874);
  spi_xmit(c2);
  delayMicroseconds(874);
  spi_xmit(c3);
  delayMicroseconds(874);
  spi_xmit(c4);
  delayMicroseconds(874);
  spi_xmit(c5);
  delayMicroseconds(874);
  spi_xmit(c6);
  delayMicroseconds(874);
  spi_xmit(c7);
}

void setup()
{
  cd = 0xBE;
  tr = 0xFF;
  mode = 0xFF;

#ifdef DEBUG
  Serial.begin(9600);
#endif

  // init SPI
  SPI.begin(SCK, MISO, MOSI, SS);

  // init timer for decooding input signal
  timer1 = timerBegin(0, 80, true); // timer1 1us tick

  // attach interrupt to listen to radio out signal
  pinMode(RADIO_OUT, INPUT);
  attachInterrupt(RADIO_OUT, INT0_vect, CHANGE); // INT0 on pin 17 any logical change

#ifdef DEBUG
  Serial.println("Begun timer and attached interrupt");
#endif

  send_package(0x74, 0xBE, 0xFE, 0xFF, 0xFF, 0xFF, 0x8F, 0x7C); // idle
  delay_ms(10);
  send_package(0x34, 0xFF, 0xFE, 0xFE, 0xFE, 0xFF, 0xFA, 0x3C); // load disc
  delay_ms(100);
  send_package(0x74, 0xBE, 0xFE, 0xFF, 0xFF, 0xFF, 0x8F, 0x7C); // idle
  delay_ms(10);

#ifdef DEBUG
  Serial.println("Sent idle/load/idle commands");
#endif
}

void loop()
{
//                disc  trk  min  sec
// send_package(0x34,cd,tr,0xFF,0xFF,0xFF,0xCF,0x3C);
#ifdef DEBUG
  Serial.println("Sent play command");
#endif
  send_package(0x34, cd, tr, 0xFF, 0xFF, mode, 0xCF, 0x3C);
  delay_ms(41);
  if (f.newCMD)
  {
    f.newCMD = 0;
    uint8_t c = getCommand(cmd);
  }
}
uint8_t getCommand(uint32_t cmd)
{
  if (((cmd >> 24) & 0xFF) == CDC_PREFIX1 && ((cmd >> 16) & 0xFF) == CDC_PREFIX2)
    if (((cmd >> 8) & 0xFF) == (0xFF ^ ((cmd) & 0xFF)))
      return (cmd >> 8) & 0xFF;
  return 0;
}