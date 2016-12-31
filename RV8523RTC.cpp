/*
 * DS1307RTC.h - library for DS1307 RTC

  Copyright (c) Michael Margolis 2009
  This library is intended to be uses with Arduino Time library functions

  The library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

  30 Dec 2009 - Initial release
  5 Sep 2011 updated for Arduino 1.0
 */


#if defined (__AVR_ATtiny84__) || defined(__AVR_ATtiny85__) || (__AVR_ATtiny2313__)
#include <TinyWireM.h>
#define Wire TinyWireM
#else
#include <Wire.h>
#endif
#include "RV8523RTC.h"

#define BIT(n)                  ( 1<<(n) )
#define BIT_SET(y, mask)        ( y |=  (mask) )
#define BIT_CLEAR(y, mask)      ( y &= ~(mask) )
#define BIT_FLIP(y, mask)       ( y ^=  (mask) )

#define RV8523_I2C_ADDR (0xD0>>1)

// Registers
#define RV8523_CTRL_1       0x00
#define RV8523_CTRL_1_CAP   7     // Must be set to logic 0 for normal operations
#define RV8523_CTRL_1_STOP  5     // 0 -> RTC time circuits running
                                  // 1 -> RTC time circuits frozen
#define RV8523_CTRL_1_SR    4     // 0 -> No SoftwareReset
                                  // 1 -> Init SoftwareReset
#define RV8523_CTRL_1_1224  3     // 0 -> 24h
                                  // 1 -> 12h
#define RV8523_CTRL_1_SIE   2     // Second interrupt enable/disable
#define RV8523_CTRL_1_AIE   1     // Alarm interrupt enable/disable
#define RV8523_CTRL_1_CIE   0     // correction interrupt enable/disable
#define RV8523_CTRL_2       0x01  // Irq stuff
#define RV8523_CTRL_3       0x02
#define RV8523_CTRL_3_BLF   2     // battery status

#define RV8523_SEC          0x03
#define RV8523_MIN          0x04
#define RV8523_HOURS        0x05
#define RV8523_DAYS         0x06
#define RV8523_WDAYS        0x07
#define RV8523_MONTHS       0x08
#define RV8523_YEARS        0x09
#define RV8523_AL_MIN       0x0A
#define RV8523_AL_HOU       0x0B
#define RV8523_AL_DAY       0x0C
#define RV8523_AL_WDA       0x0D
#define RV8523_OFFSET       0x0E
#define RV8523_CLKOUT       0x0F
#define RV8523_A_CLK        0x10
#define RV8523_A_           0x11
#define RV8523_B_CLK        0x12
#define RV8523_C            0x13



RV8523RTC::RV8523RTC()
{
  Wire.begin();
}

// PUBLIC FUNCTIONS
time_t RV8523RTC::get()   // Aquire data from buffer and convert to time_t
{
  tmElements_t tm;
  if (read(tm) == false) return 0;
  return(makeTime(tm));
}

bool RV8523RTC::set(time_t t)
{
  tmElements_t tm;
  breakTime(t, tm);
  return write(tm);
}

// Aquire data from the RTC chip in BCD format
/*
Recommended method for reading the time:
1. Send a START condition and the slave address for write (D0h)
2. Set the address pointer to 3 (Seconds) by sending 03h
3. Send a RE-START condition (STOP followed by START)
4. Send the slave address for read (D1h)
5. Read the seconds
6. Read the minutes
7. Read the hours
8. Read the days
9. Read the weekdays
10. Read the months
11. Read the years
12. Send a STOP condition
*/

bool RV8523RTC::read(tmElements_t &tm)
{
  uint8_t sec;
  Wire.beginTransmission(RV8523_I2C_ADDR);
  // send addr of secconds register
#if ARDUINO >= 100
  Wire.write((uint8_t)RV8523_SEC);
#else
  Wire.send(RV8523_SEC);
#endif
  if (Wire.endTransmission() != 0) {
    exists = false;
    return false;
  }
  exists = true;

  /* request the 7 data fields   (secs, min, hr, dow, date, mth, yr)
  * RV8523_SEC RV8523_MIN RV8523_HOURS RV8523_DAYS RV8523_WDAYS RV8523_MONTHS RV8523_YEARS
  */
  Wire.requestFrom(RV8523_I2C_ADDR, tmNbrFields);
  if (Wire.available() < tmNbrFields){
    return false;
  }
#if ARDUINO >= 100
  sec = Wire.read();
  tm.Second = bcd2dec(sec & 0x7f);
  tm.Minute = bcd2dec(Wire.read() );
  tm.Hour =   bcd2dec(Wire.read() & 0x3f);  // mask assumes 24hr clock
  tm.Day = bcd2dec(Wire.read() );
  tm.Wday = bcd2dec(Wire.read() );
  tm.Month = bcd2dec(Wire.read() );
  tm.Year = y2kYearToTm((bcd2dec(Wire.read())));
#else
  sec = Wire.receive();
  tm.Second = bcd2dec(sec & 0x7f);
  tm.Minute = bcd2dec(Wire.receive() );
  tm.Hour =   bcd2dec(Wire.receive() & 0x3f);  // mask assumes 24hr clock
  tm.Day = bcd2dec(Wire.receive() );
  tm.Wday = bcd2dec(Wire.receive() );
  tm.Month = bcd2dec(Wire.receive() );
  tm.Year = y2kYearToTm((bcd2dec(Wire.receive())));
#endif
  if (sec & 0x80)
  {
    return false; // Clock integrity is not guaranteed
  }
  return true;
}

bool RV8523RTC::write(tmElements_t &tm)
{
  // To eliminate any potential race conditions,
  // stop the clock before writing the values,
  // then restart it after.
  Wire.beginTransmission(RV8523_I2C_ADDR);
#if ARDUINO >= 100
  Wire.write((uint8_t)RV8523_SEC); // reset register pointer
  Wire.write(dec2bcd(tm.Second));
  Wire.write(dec2bcd(tm.Minute));
  Wire.write(dec2bcd(tm.Hour));      // sets 24 hour format
  Wire.write(dec2bcd(tm.Day));
  Wire.write(dec2bcd(tm.Wday));
  Wire.write(dec2bcd(tm.Month));
  Wire.write(dec2bcd(tmYearToY2k(tm.Year)));
#else
  Wire.send(RV8523_SEC); // reset register pointer
  Wire.send(dec2bcd(tm.Second));
  Wire.send(dec2bcd(tm.Minute));
  Wire.send(dec2bcd(tm.Hour));      // sets 24 hour format
  Wire.send(dec2bcd(tm.Day));
  Wire.send(dec2bcd(tm.Wday));
  Wire.send(dec2bcd(tm.Month));
  Wire.send(dec2bcd(tmYearToY2k(tm.Year)));
#endif
  if (Wire.endTransmission() != 0) {
    exists = false;
    return false;
  }
  exists = true;
  return true;
}

unsigned char RV8523RTC::isRunning()
{
  Wire.beginTransmission(RV8523_I2C_ADDR);
#if ARDUINO >= 100
  Wire.write((uint8_t)RV8523_SEC);
#else
  Wire.send(RV8523_SEC);
#endif
  Wire.endTransmission();

  // Just fetch the seconds register and check the top bit
  Wire.requestFrom(RV8523_I2C_ADDR, 1);
#if ARDUINO >= 100
  return !(Wire.read() & 0x80);
#else
  return !(Wire.receive() & 0x80);
#endif
}

void RV8523RTC::setCalibration(char calValue)
{
  unsigned char calReg = abs(calValue) & 0x1f;
  if (calValue >= 0) calReg |= 0x20; // S bit is positive to speed up the clock
  Wire.beginTransmission(RV8523_I2C_ADDR);
#if ARDUINO >= 100
  Wire.write((uint8_t)RV8523_OFFSET); // Point to calibration register
  Wire.write(calReg);
#else
  Wire.send(RV8523_OFFSET); // Point to calibration register
  Wire.send(calReg);
#endif
  Wire.endTransmission();
}

char RV8523RTC::getCalibration()
{
  Wire.beginTransmission(RV8523_I2C_ADDR);
#if ARDUINO >= 100
  Wire.write((uint8_t)RV8523_OFFSET);
#else
  Wire.send(RV8523_OFFSET);
#endif
  Wire.endTransmission();

  Wire.requestFrom(RV8523_I2C_ADDR, 1);
#if ARDUINO >= 100
  unsigned char calReg = Wire.read();
#else
  unsigned char calReg = Wire.receive();
#endif
  char out = calReg & 0x1f;
  if (!(calReg & 0x20)) out = -out; // S bit clear means a negative value
  return out;
}

// PRIVATE FUNCTIONS

// Convert Decimal to Binary Coded Decimal (BCD)
uint8_t RV8523RTC::dec2bcd(uint8_t num)
{
  return ((num/10 * 16) + (num % 10));
}

// Convert Binary Coded Decimal (BCD) to Decimal
uint8_t RV8523RTC::bcd2dec(uint8_t num)
{
  return ((num/16 * 10) + (num % 16));
}

bool RV8523RTC::exists = false;

RV8523RTC RTC = RV8523RTC(); // create an instance for the user
