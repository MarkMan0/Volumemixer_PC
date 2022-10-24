#pragma once


#include <Windows.h>
#include <WinBase.h>
#include <stdint.h>
#include <string_view>
#include <string>

/// @brief Wraps a windows COM port to ease working with it
class SerialPortWrapper {
public:
  SerialPortWrapper(std::wstring_view port, int baud);
  SerialPortWrapper(int port, int baud);

  SerialPortWrapper(const SerialPortWrapper&) = delete;
  SerialPortWrapper& operator=(const SerialPortWrapper&) = delete;

  ~SerialPortWrapper();

  void open();
  void close();

  /// @brief  Check if port is open and ready
  bool operator()() const;

  int write(const uint8_t*, size_t);
  int read(uint8_t*, size_t);
  void flush();
  char get_char();
  void put_char(char);


private:
  const std::wstring port_name_;
  const int baud_{};
  HANDLE com_handle_ = INVALID_HANDLE_VALUE;
  DCB dcb_old_;
  DCB dcb_new_;
  COMMTIMEOUTS timeout_old_;
  COMMTIMEOUTS timeout_new_;
};
