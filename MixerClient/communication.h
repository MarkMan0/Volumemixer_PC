#pragma once
#include <cstdint>
#include "SerialPortWrapper/SerialPortWrapper.h"
#include <vector>
#include <iostream>

#define DO_DEBUG
#ifdef DO_DEBUG
  #define DEBUG_PRINT(ARG)  std::cout << ARG;
  #define DEBUG_WPRINT(ARG) std::wcout << ARG;
#else
  #define DEBUG_PRINT(ARG)
  #define DEBUG_WPRINT(ARG)
#endif

namespace mixer {
  enum commands : uint8_t {
    LOAD_ALL = 0x01,
    READ_IMG = 0x02,
    SET_VOLUME = 0x03,
    ECHO = 0x04,
    SET_MUTE = 0x05,
    QUERY_CHANGES = 0x06,
    RESPONSE_OK_0 = 0xA0,
    RESPONSE_OK_1 = 0xA1,
  };
}

std::vector<uint8_t> wait_data(SerialPortWrapper&, size_t);

bool serial_comm(SerialPortWrapper&);

void respond_load(SerialPortWrapper&);
void respond_img(SerialPortWrapper&);
void respond_set_volume(SerialPortWrapper&);
void respond_echo(SerialPortWrapper&);
void respond_mute(SerialPortWrapper&);
void respond_query_changes(SerialPortWrapper&);