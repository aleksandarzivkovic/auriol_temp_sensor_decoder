/* 
 * 433MHz Auriol temperature sensor data decoder
 * 
 * Arduino Nano application that decodes Auriol temperature sensor data from 
 * 433MHz receiver. Receiver output is connected to D8 input pin. Serial data
 * is delivered at D0 pin (UART Tx pin) at 115.2kbps.
 * Data delivered has 24 bits:
 *  - bits 0-7   - Random value (never 0x00) set when inserting batteries
 *  - bit 8      - Manual/automatic data send (assume '1' means 'manual' - can someone confirm?)
 *  - bit 9      - Low battery; also causes a beep from the receiver. Low battery is only cleared by resetting the sender/receiver
 *  - bits 10-11 - channel
 *  - bits 12-23 - temperature in 'degC x 10' at two's complement format
 */

#include <Wire.h>

// define timings for one and zero bits (in number of ticks - 1 tick = 0.5us)
#define BIT_ZERO_MIN 4000
#define BIT_ZERO_MAX 5000
#define BIT_ONE_MIN  8000
#define BIT_ONE_MAX  9000

// data decoding constants
typedef enum {
  ZERO,
  ONE,
  ERR_INVALID,
  ERR_MISSED,
  READ
} data_bits_t;

// shared variable between interrupt routine and loop function
static data_bits_t m_cData = READ;

// circular shift (https://en.wikipedia.org/wiki/Circular_shift)
uint8_t rotl8 (uint8_t value) {
  return value << 1 | value >> 7;
}

//============================================================================
void setup() {
  //programming timer1 for input capture interrupt
  noInterrupts();
  TCCR1A = 0;  //initialize timer1 mode
  TCCR1B = 0;
  TCCR1B |= 0b11000010;  //set 8 prescaler (one tick is 0.5uS), rising edge, noise canceler
  TIMSK1 |= 0b00100000;  //enable input capture interrupt
  TCNT1 = 0;             //initialize timer/counter1 register
  interrupts();

  // configure serial communication
  Serial.begin(115200);
}

//============================================================================
ISR(TIMER1_CAPT_vect)  //input capture interrupt ISR
{
  static uint16_t _uiCurrentCapture = 0;
  static uint16_t _uiPreviousCapture = 0;
  uint16_t _uiTimeDiff;

  _uiCurrentCapture = ICR1;
  _uiTimeDiff = _uiCurrentCapture - _uiPreviousCapture;
  _uiPreviousCapture = _uiCurrentCapture;
  
  if (m_cData != READ) {
    m_cData = ERR_MISSED;  // loop didn't read last m_cData from this interrupt routine
  } else if ((_uiTimeDiff > BIT_ZERO_MIN)&&(_uiTimeDiff < BIT_ZERO_MAX)) {
    m_cData = ZERO;
  } else if ((_uiTimeDiff > BIT_ONE_MIN)&&(_uiTimeDiff < BIT_ONE_MAX)) {
    m_cData = ONE;
  } else {
    m_cData = ERR_INVALID;
  }
}

//============================================================================
void loop() {
  data_bits_t _tmp;
  static uint8_t _ucByte = 0;
  static uint8_t _ucArray[4];
  static uint8_t _ucIdx = 0;
  static uint8_t m_cBitsAcquired = 0;
  uint32_t m_ulData = 0;
  uint8_t m_ucCRC = 0;
  uint8_t _ucTmp = 0;
  
  if (m_cData != READ) {
    // get data from interrupt routine
    _tmp = m_cData;
    m_cData = READ;

    // add bits to byte or discard if wrong measurement
    if (_tmp == ONE){
      _ucByte = _ucByte << 1;
      _ucByte = _ucByte | 0x01;
      m_cBitsAcquired++;
    } else if (_tmp == ZERO){
      _ucByte = _ucByte << 1;
      _ucByte = _ucByte & 0xFE;
      m_cBitsAcquired++;
    }  else {
      m_cBitsAcquired = 0;
      _ucIdx = 0;
    }
    
    // copy acquired bits to array byte
    if (m_cBitsAcquired == 8){
      _ucArray[_ucIdx++] = _ucByte;
      m_cBitsAcquired = 0;
      _ucByte = 0;

      // message is received
      if(_ucIdx > 3) {
        _ucIdx = 0;

        m_ulData = (uint32_t)_ucArray[2];
        m_ulData |= ((uint32_t)_ucArray[1]) << 8;
        m_ulData |= ((uint32_t)_ucArray[0]) << 16;
        m_ucCRC = 0;

        // calculate checksum (https://github.com/merbanan/rtl_433/issues/45#issuecomment-204401154)
        for(int i = 0; i<40; i++) {
          _ucTmp = 0;

          if (m_ucCRC & 0x80)
            m_ucCRC ^= 0x18;
          m_ucCRC = rotl8 (m_ucCRC);
          if (m_ulData & 0x00800000)
            _ucTmp = 0x01;
          m_ucCRC ^= _ucTmp;
          m_ulData <<= 1;
          m_ulData &= 0x00FFFFFF;
        }
        
        // send over UART if checksum is correct
        if (m_ucCRC == _ucArray[3])
          Serial.write(_ucArray,3);
      }
    }
  }
}