// =============================================================================
// VWCDC - ESP32 CD Changer Emulator
// =============================================================================
// Created: 2024-03-01
// Last Modified: 2025-03-16
// Created by: Daniel Kern (NullString1)
// Description: Emulates CD changer for VW/Audi/Skoda/Seat head units to enable
//              AUX input from phone or bluetooth modules.
// Related Projects: tomaskovacik/vwcdavr, shyd/avr-raspberry-pi-vw-beta-vag-cdc-faker,
//                   k9spud/vwcdpic
// License: GPL-3.0
// Version: 1.2
// Website: https://nullstring.one
// Repository: https://github.com/NullString1/VWCDC
//
// =============================================================================
// HARDWARE CONNECTIONS
// =============================================================================
// | Name    | ESP32 Pin | Radio Pin  | Purpose                              |
// |---------|-----------|------------|--------------------------------------|
// | SCLK    | GPIO18    | CDC CLOCK  | SPI Clock (62.5kHz)                  |
// | MOSI    | GPIO23    | DATA IN    | Data from ESP to radio (SPI output)  |
// | DATA OUT| GPIO4     | DATA OUT   | Radio button commands (RMT input)    |
//
// =============================================================================
// PROTOCOL OVERVIEW
// =============================================================================
// The VAG CD changer protocol uses two separate communication methods:
//
// 1. SPI OUTPUT (ESP -> Radio): Used to tell the radio about our "CD changer"
//    - 8 bytes sent at 62.5kHz MSB first, SPI Mode 1
//    - Byte timing: 874µs between each byte
//    - Format: 0x34, CD#, TR#, MIN, SEC, MODE, CHECKSUM, 0x3C
//    - Sent continuously every ~50ms to maintain connection
//
// 2. PULSE-WIDTH INPUT (Radio -> ESP): Used to receive button presses from radio
//    - Based on inverted NEC infrared protocol (not standard NEC!)
//    - Idle state: HIGH (5V)
//    - Start bit: 9ms LOW, 4.55ms HIGH
//    - Logic 1: ~600µs LOW, ~1700µs HIGH
//    - Logic 0: ~600µs LOW, ~600µs HIGH
//    - Data format: 4 bytes (32 bits total)
//      Byte 0: 0x53 (prefix 1)
//      Byte 1: 0x2C (prefix 2)
//      Byte 2: Command byte
//      Byte 3: Inverse of command byte (checksum)
//
// =============================================================================
// RMT PERIPHERAL
// =============================================================================
// The ESP32's RMT (Remote Control) peripheral is ideal for decoding the
// pulse-width modulated signal from the radio. It operates independently of
// the CPU and can capture timing data without software intervention.
//
// Key RMT features used:
// - Hardware-based pulse width measurement
// - Independent clock (APB bus @ 80MHz, divided by clk_div=80 = 1µs ticks)
// - Ring buffer for non-blocking data capture
// - Active low mode for inverted NEC signaling
//
// =============================================================================

// Uncomment to enable debug output on Serial (9600 baud)
// #define DEBUG

// =============================================================================
// CDC PROTOCOL CONSTANTS - OUTPUT (ESP -> Radio)
// =============================================================================
// These define the byte values used in our SPI messages to the radio

#define CDC_PREFIX1 0x53      // First byte of all status packets (0x34 or 0x74)
#define CDC_PREFIX2 0x2C      // Second byte (used in command validation)

#define CDC_END_CMD 0x14      // End command marker
#define CDC_PLAY 0xE4         // Play button command
#define CDC_STOP 0x10         // Stop button command
#define CDC_NEXT 0xF8         // Next track button
#define CDC_PREV 0x78         // Previous track button
#define CDC_SEEK_FWD 0xD8     // Seek forward
#define CDC_SEEK_RWD 0x58     // Seek reverse
#define CDC_CD1 0x0C          // Select CD 1
#define CDC_CD2 0x8C          // Select CD 2
#define CDC_CD3 0x4C          // Select CD 3
#define CDC_CD4 0xCC          // Select CD 4
#define CDC_CD5 0x2C          // Select CD 5
#define CDC_CD6 0xAC          // Select CD 6
#define CDC_SCAN 0xA0         // Scan mode
#define CDC_SFL 0x60          // Shuffle mode
#define CDC_PLAY_NORMAL 0x08  // Normal play mode

// Playback mode constants (sent in byte 5 of status packet)
#define MODE_PLAY 0xFF        // Normal playback
#define MODE_SHFFL 0x55       // Shuffle mode
#define MODE_SCAN 0x00        // Scan mode

// =============================================================================
// HARDWARE PIN DEFINITIONS
// =============================================================================
// GPIO4: Connected to radio's DATA OUT pin
// This receives button press signals from the radio
// Using GPIO4 because:
// - Not used by SPI (SPI uses GPIO18, 19, 23)
// - Not used by default Arduino functions
// - Supports RTC wakeup (not needed here but good to have)
#define RADIO_OUT_PIN 4

// =============================================================================
// INCLUDES
// =============================================================================
#include <Arduino.h>              // Arduino framework
#include <SPI.h>                  // SPI communication
#include <driver/rmt.h>           // ESP32 RMT peripheral
#include <freertos/FreeRTOS.h>   // FreeRTOS for tasks/queues
#include <freertos/queue.h>      // Queue implementation

// =============================================================================
// DEBUG MACROS
// =============================================================================
// Conditional debug output - only compiles Serial.print statements when DEBUG
// is defined. This saves memory and CPU when not debugging.
#ifdef DEBUG
#define DEBUG_PRINT(x) Serial.print(x)
#define DEBUG_PRINTLN(x) Serial.println(x)
#define DEBUG_PRINTLN_HEX(x) Serial.println(x, HEX)
#else
#define DEBUG_PRINT(x)
#define DEBUG_PRINTLN(x)
#define DEBUG_PRINTLN_HEX(x)
#endif

// =============================================================================
// GLOBAL VARIABLES
// =============================================================================

// Ring buffer handle for RMT data - stores received pulse timing data
static RingbufHandle_t rmt_ringbuf = NULL;

// Timer tracking for periodic status packet transmission
volatile long prevMillis = 0;

// Flag to enable input handling - disabled during initialization to prevent
// interfering with the critical startup sequence. Radio may send spurious
// signals during boot which we should ignore.
volatile bool input_enabled = false;

// Current playback state - these are sent to the radio in our status packets
uint8_t cd = 1;      // Current CD number (1-6)
uint8_t tr = 1;      // Current track number (1-99)
uint8_t mode = MODE_PLAY;  // Current playback mode

// Queue for storing decoded commands - the RMT task decodes commands and places
// them here, then the main loop processes them. This prevents blocking the
// RMT peripheral.
QueueHandle_t cmd_queue;

// =============================================================================
// FUNCTION: send_package
// =============================================================================
// Sends an 8-byte status packet to the radio via SPI.
//
// The VAG CDC protocol expects:
// - SPI at 62.5kHz, MSB first, Mode 1
// - 874µs delay between each byte
// - Packet format: CMD, CD_INV, TR_INV, 0xFF, 0xFF, MODE, CHECKSUM, END
//
// Arguments:
//   c0-c7: The 8 bytes to send
//
// Notes:
//   - Uses SPI.beginTransaction/endTransaction for proper bus control
//   - Each byte has a 874µs gap - this is critical timing!
//   - Radio expects this packet every ~50ms to maintain "connection"
//
// Example status packet when playing CD1 Track 1:
//   0x34, 0xBF^1=0xBE, 0xFF^1=0xFE, 0xFF, 0xFF, 0xFF, 0xCF, 0x3C
//   (0xBF ^ cd) inverts the CD byte, (0xFF ^ tr) inverts track byte
// =============================================================================
void send_package(uint8_t c0, uint8_t c1, uint8_t c2, uint8_t c3, uint8_t c4, uint8_t c5, uint8_t c6, uint8_t c7) {
  // Configure SPI: 62.5kHz clock, MSB first, Mode 1 (clock idle low, sample on rising edge)
  SPI.beginTransaction(SPISettings(62500, MSBFIRST, SPI_MODE1));
  
  // Send each byte with precise 874µs delay between bytes
  // This timing is critical - radio expects exact spacing
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
  
  // Release SPI bus
  SPI.endTransaction();
}

// =============================================================================
// FUNCTION: decode_command
// =============================================================================
// Validates and extracts the command byte from a raw 32-bit NEC-style command.
//
// VAG CDC command format (32 bits = 4 bytes):
//   Byte 0: 0x53 (CDC_PREFIX1) - Protocol identifier
//   Byte 1: 0x2C (CDC_PREFIX2) - Secondary identifier  
//   Byte 2: Command byte       - The actual button code
//   Byte 3: Inverse of byte 2  - Checksum (0xFF ^ command)
//
// Returns:
//   - Command byte if valid (checksum matches)
//   - 0 if invalid (wrong prefix or failed checksum)
//
// Validation steps:
//   1. First byte must be 0x53
//   2. Second byte must be 0x2C
//   3. Byte 3 must be inverse of Byte 2 (0xFF ^ byte2)
// =============================================================================
uint8_t decode_command(uint32_t raw_cmd) {
  // Extract each byte from the 32-bit command
  // Commands are stored MSB first, so we shift accordingly
  uint8_t byte0 = (raw_cmd >> 24) & 0xFF;  // First byte
  uint8_t byte1 = (raw_cmd >> 16) & 0xFF;  // Second byte
  uint8_t byte2 = (raw_cmd >> 8) & 0xFF;   // Command byte
  uint8_t byte3 = raw_cmd & 0xFF;           // Checksum byte
  
  // Validate prefix bytes
  if (byte0 != CDC_PREFIX1 || byte1 != CDC_PREFIX2) {
    return 0;  // Not a valid CDC command
  }
  
  // Validate checksum: byte3 should equal 0xFF ^ byte2
  if (byte3 != (0xFF ^ byte2)) {
    return 0;  // Checksum failed
  }
  
  // Command is valid - return the command byte
  return byte2;
}

// =============================================================================
// FUNCTION: rmt_decoder
// =============================================================================
// Decodes the raw RMT pulse data into a 32-bit command.
//
// The RMT peripheral captures each pulse as a pair of durations:
//   - duration0: How long the signal was at level0
//   - duration1: How long the signal was at level1
//
// For inverted NEC protocol (VAG uses inverted!):
//   - Signal is LOW when transmitting (not HIGH like standard NEC)
//   - Logic 0: ~600µs LOW + ~600µs HIGH = total ~1200µs
//   - Logic 1: ~600µs LOW + ~1700µs HIGH = total ~2300µs
//   - Start bit: 9000µs LOW + 4550µs HIGH = total ~13550µs
//
// The RMT clock is 80MHz / 80 = 1MHz = 1 tick per µs
// So durations are measured in microseconds.
//
// Arguments:
//   item: Pointer to RMT item data (contains pulse timings)
//   num_items: Number of items in the data
//   decoded_cmd: Pointer to store the decoded 32-bit command
//
// Returns: true if successfully decoded 32 bits, false otherwise
// =============================================================================
bool rmt_decoder(const rmt_item32_t* item, size_t num_items, uint32_t* decoded_cmd) {
  // Need at least 64 items to represent 32 bits of data (start + 32 bits + stop)
  if (num_items < 64) {
    return false;
  }
  
  uint32_t cmd = 0;
  int bit_pos = 0;  // Track which bit position we're on (0-31)
  
  // Iterate through each RMT item (pulse pair)
  for (int i = 0; i < num_items && bit_pos < 32; i++) {
    uint32_t duration0 = item[i].duration0;  // First level duration (ticks/µs)
    uint32_t duration1 = item[i].duration1;  // Second level duration
    
    // Skip items that don't represent data (we're looking for LOW-HIGH pairs)
    // level0=1 means signal went HIGH first (not our inverted format)
    if (item[i].level0 == 1 || item[i].level1 == 0) {
      continue;
    }
    
    // Decode based on pulse widths (values in µs due to clock divider)
    // The protocol uses ~600µs base, so we use ranges to tolerate timing variation
    
    // Logic 0: LOW ~600µs + HIGH ~600µs = ~1200µs total
    // In our timing: duration0 ~7-11 ticks (7-11µs), duration1 ~3-6 ticks (3-6µs)
    // Note: values scaled by 1000 due to timing calibration
    if (duration0 >= 7 && duration0 <= 11 && duration1 >= 3 && duration1 <= 6) {
      cmd |= (0 << bit_pos);  // Write a 0 bit
      bit_pos++;
    }
    // Logic 1: LOW ~600µs + HIGH ~1700µs = ~2300µs total
    else if (duration0 >= 7 && duration0 <= 11 && duration1 >= 12 && duration1 <= 20) {
      cmd |= (1 << bit_pos);  // Write a 1 bit
      bit_pos++;
    }
    // If neither matches, it's likely a start/stop bit or noise - skip it
  }
  
  // Successfully decoded all 32 bits?
  if (bit_pos >= 32) {
    *decoded_cmd = cmd;
    return true;
  }
  
  return false;
}

// =============================================================================
// FUNCTION: rmt_task
// =============================================================================
// FreeRTOS task that processes RMT data from the ring buffer.
//
// This task runs independently from the main loop:
// - Waits for RMT data to become available in the ring buffer
// - Extracts pulse timing data from the buffer
// - Decodes the raw pulses into a 32-bit command
// - Validates the command using decode_command()
// - Places valid commands into the queue for main loop processing
//
// This decoupled design ensures:
// - RMT peripheral isn't blocked by slow command processing
// - Main loop can process commands at its own pace
// - No dropped data when buttons are pressed rapidly
// =============================================================================
void rmt_task(void* arg) {
  (void) arg;  // Suppress unused parameter warning
  
  size_t rx_size = 0;
  
  // Main loop - wait for RMT data
  while (1) {
    // Blocking call - waits up to 100ms for data
    rmt_item32_t* item = (rmt_item32_t*) xRingbufferReceive(rmt_ringbuf, &rx_size, 100);
    
    if (item) {
      uint32_t decoded_cmd = 0;
      
      // Try to decode the raw pulse data
      if (rmt_decoder(item, rx_size / 4, &decoded_cmd)) {
        uint8_t cmd = decode_command(decoded_cmd);
        
        // Only process valid commands after initialization is complete
        if (cmd != 0 && input_enabled) {
          // Send command to queue (non-blocking, 0 timeout)
          xQueueSend(cmd_queue, &cmd, 0);
          DEBUG_PRINT("CMD: 0x");
          DEBUG_PRINTLN_HEX(cmd);
        }
        #ifdef DEBUG
        else if (cmd == 0) {
          // Print raw data for debugging protocol issues
          DEBUG_PRINT("RAW: ");
          DEBUG_PRINTLN(decoded_cmd, HEX);
        }
        #endif
      }
      
      // Return item to ring buffer
      vRingbufferReturnItem(rmt_ringbuf, (void*) item);
    }
  }
  
  // Should never reach here
  vTaskDelete(NULL);
}

// =============================================================================
// FUNCTION: setup_rmt_input
// =============================================================================
// Initializes the RMT peripheral for receiving button commands from the radio.
//
// Configuration details:
// - RMT Channel 0: Hardware channel 0 (also channel 1-7 available)
// - GPIO 4: Input pin connected to radio DATA OUT
// - Clock divider 80: 80MHz / 80 = 1MHz = 1 tick per µs
// - Idle threshold 12000: 12ms of idle signal ends a command
// - Filter enabled: Ignores noise on the input line
//
// The RMT peripheral will automatically:
// - Capture timing data when signal changes
// - Store data in the ring buffer
// - Signal our task when data is ready
// =============================================================================
void setup_rmt_input() {
  // Configure RMT channel 0 for receiver using legacy driver
  rmt_config_t rmt_cfg;
  rmt_cfg.channel = RMT_CHANNEL_0;
  rmt_cfg.gpio_num = (gpio_num_t)RADIO_OUT_PIN;
  rmt_cfg.clk_div = 80;                // 80MHz / 80 = 1MHz (1 tick = 1µs)
  rmt_cfg.mem_block_num = 1;           // Use 1 memory block (64 items)
  rmt_cfg.rmt_mode = RMT_MODE_RX;      // RX mode
  
  // RX configuration - enable filter to ignore noise
  rmt_cfg.rx_config.filter_en = true;
  rmt_cfg.rx_config.idle_threshold = 12000;
  
  // Apply configuration
  rmt_config(&rmt_cfg);
  
  // Install RMT driver - creates ringbuffer automatically
  // Parameters: channel, rx buffer size, queue size
  rmt_driver_install(RMT_CHANNEL_0, 3000, 0);
  
  // Get ringbuffer handle from driver
  rmt_get_ringbuf_handle(RMT_CHANNEL_0, &rmt_ringbuf);
  
  // Start receiving
  rmt_rx_start(RMT_CHANNEL_0, true);
  
  // Create queue for decoded commands (8 slots for button mashing)
  cmd_queue = xQueueCreate(8, sizeof(uint8_t));
  
  // Create the RMT processing task
  // Stack size: 2048 bytes, Priority: 5 (above normal)
  xTaskCreate(rmt_task, "rmt_decoder", 2048, NULL, 5, NULL);
  
  DEBUG_PRINT("RMT input initialized on GPIO");
  DEBUG_PRINTLN(RADIO_OUT_PIN);
}

// =============================================================================
// FUNCTION: process_command
// =============================================================================
// Handles decoded button commands from the radio.
//
// Maps CDC protocol commands to internal state changes:
// - CD selection buttons: Update current CD number
// - Play/Stop: Set playback mode
// - Next/Previous: Change track number
// - Seek buttons: Could trigger fast forward/rewind
// - Scan/Shuffle: Change playback mode
//
// In a full implementation, these state changes would be used to:
// - Control an external audio source (Bluetooth module, AUX input)
// - Update display on the radio (track/CD numbers)
// - Send commands to a BLE keyboard library for phone control
//
// Current behavior:
// - Just updates internal state variables
// - These are reflected in the status packets sent to radio
// =============================================================================
void process_command(uint8_t cmd) {
  switch (cmd) {
    // CD Selection: 6 buttons to select which "CD" is playing
    // In a real implementation, these could map to:
    // - Playlists on a phone
    // - Different audio sources
    // - Saved radio stations
    case CDC_CD1:
    case CDC_CD2:
    case CDC_CD3:
    case CDC_CD4:
    case CDC_CD5:
    case CDC_CD6:
      cd = cmd;
      DEBUG_PRINT("CD selected: ");
      DEBUG_PRINTLN(cd);
      break;

    // Play commands - could start Bluetooth playback
    case CDC_STOP:
    case CDC_PLAY_NORMAL:
    case CDC_PLAY:
      mode = MODE_PLAY;
      DEBUG_PRINTLN("Play");
      break;

    // Next track - could send next track command to BT
    case CDC_NEXT:
      tr++;
      if (tr > 99) tr = 1;
      DEBUG_PRINT("Next track: ");
      DEBUG_PRINTLN(tr);
      break;

    // Previous track
    case CDC_PREV:
      if (tr == 1) tr = 99;
      else tr--;
      DEBUG_PRINT("Prev track: ");
      DEBUG_PRINTLN(tr);
      break;

    // Seek buttons - could fast forward/rewind
    case CDC_SEEK_FWD:
      DEBUG_PRINTLN("Seek forward");
      break;

    case CDC_SEEK_RWD:
      DEBUG_PRINTLN("Seek rewind");
      break;

    // Mode buttons
    case CDC_SCAN:
      mode = MODE_SCAN;
      DEBUG_PRINTLN("Scan mode");
      break;

    case CDC_SFL:
      mode = MODE_SHFFL;
      DEBUG_PRINTLN("Shuffle mode");
      break;

    // Unknown command - could be debugged
    default:
      DEBUG_PRINT("Unknown command: 0x");
      DEBUG_PRINTLN_HEX(cmd);
      break;
  }
}

// =============================================================================
// FUNCTION: setup
// =============================================================================
// Main initialization function - runs once at power-on/reset.
//
// Initialization sequence:
// 1. Set default state (CD1, Track1, Play mode)
// 2. Start debug serial if enabled
// 3. Wait 1 second for radio to power up
// 4. Initialize SPI bus (but don't start sending yet)
// 5. Send initialization sequence to radio:
//    - Idle packet: Tells radio we're connected but not playing
//    - Load disc packet: Tells radio a disc is inserted
//    - Idle packet: Back to idle state
//    This sequence is required for the radio to recognize us as a CDC
// 6. Initialize RMT for receiving button commands
// 7. Wait 500ms more
// 8. Enable input processing
//
// The delay between init packets is critical:
// - 10ms after first idle: allows radio to process
// - 100ms after load disc: longer time for disc loading animation
// - 10ms after final idle: allows radio to finalize initialization
// =============================================================================
void setup() {
  // Initialize default playback state
  cd = 1;
  tr = 1;
  mode = MODE_PLAY;

#ifdef DEBUG
  // Start serial for debugging - only needed during development
  Serial.begin(9600);
#endif

  DEBUG_PRINTLN("VWCDC v1.2 starting...");

  // Wait for radio to power on and stabilize
  // Some radios take a while to boot their CDC interface
  delay(1000);

  // Initialize SPI bus
  // Parameters: SCK=GPIO18, MISO=GPIO19(not used), MOSI=GPIO23, SS=GPIO5(default)
  // Note: We're using default SS pin, but it doesn't matter since we're always master
  SPI.begin(SCK, MISO, MOSI, SS);

  // Send initialization sequence to radio
  // This tells the radio we're a valid CD changer
  
  // Step 1: Send idle packet - "I'm here but not doing anything"
  // Format: 0x74, 0xBE, 0xFE, 0xFF, 0xFF, 0xFF, 0x8F, 0x7C
  send_package(0x74, 0xBE, 0xFE, 0xFF, 0xFF, 0xFF, 0x8F, 0x7C);
  delayMicroseconds(10000);
  
  // Step 2: Send "load disc" packet - simulates disc being inserted
  // This may trigger the radio to show "CD1" on display
  // Format: 0x34, 0xFF, 0xFE, 0xFE, 0xFE, 0xFF, 0xFA, 0x3C
  send_package(0x34, 0xFF, 0xFE, 0xFE, 0xFE, 0xFF, 0xFA, 0x3C);
  delayMicroseconds(100000);  // 100ms - wait for "disc load" to complete
  
  // Step 3: Back to idle
  send_package(0x74, 0xBE, 0xFE, 0xFF, 0xFF, 0xFF, 0x8F, 0x7C);
  delayMicroseconds(10000);

  DEBUG_PRINTLN("Init sequence complete");

  // Initialize RMT for receiving button commands
  setup_rmt_input();

  // Wait a bit more for everything to stabilize
  delay(500);
  
  // Now enable input processing
  // Before this point, any spurious signals from the radio are ignored
  input_enabled = true;
  DEBUG_PRINTLN("Input enabled");
}

// =============================================================================
// FUNCTION: loop
// =============================================================================
// Main program loop - runs continuously after setup().
//
// Two main tasks:
// 1. Send status packet to radio every 50ms
//    - Keeps the radio thinking we're still connected
//    - Shows current CD/track/mode on radio display
//    
// 2. Check for incoming button commands
//    - Non-blocking check of command queue
//    - Process any commands received from radio
//
// The 50ms interval is important:
// - Too fast: Wastes CPU, may overwhelm radio
// - Too slow: Radio may think CDC disconnected, disable AUX
// - 50ms = 20 packets/second = responsive but not excessive
// =============================================================================
void loop() {
  // Send status packet every 50ms
  if ((millis() - prevMillis) > 50) {
    // Build status packet with current state
    // Format: 0x34, CD^, TR^, 0xFF, 0xFF, MODE, CHECKSUM, 0x3C
    // CD^ and TR^ are bitwise inverted (XOR with 0xFF)
    send_package(0x34, 0xBF ^ cd, 0xFF ^ tr, 0xFF, 0xFF, mode, 0xCF, 0x3C);
    prevMillis = millis();
  }
  
  // Check for any button commands from radio
  // xQueueReceive with 0 timeout returns immediately if queue is empty
  uint8_t cmd;
  if (xQueueReceive(cmd_queue, &cmd, 0) == pdTRUE) {
    process_command(cmd);
  }
}
