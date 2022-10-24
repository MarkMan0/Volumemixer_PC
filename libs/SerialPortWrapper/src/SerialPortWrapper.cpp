#include "SerialPortWrapper/SerialPortWrapper.h"
#include <cstdio>

SerialPortWrapper::SerialPortWrapper(std::wstring_view port, int baud) : port_name_(port), baud_(baud) {
}

SerialPortWrapper::SerialPortWrapper(int port, int baud) : SerialPortWrapper(L"//.////COM" + std::to_wstring(port), baud) {
}

void SerialPortWrapper::open() {
  com_handle_ = CreateFile(port_name_.c_str(), GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, 0, 0);
  if (com_handle_ == INVALID_HANDLE_VALUE) return;

  GetCommTimeouts(com_handle_, &timeout_old_);

  timeout_new_ = timeout_old_;

  timeout_new_.ReadIntervalTimeout = 500;
  timeout_new_.ReadTotalTimeoutConstant = 10;
  timeout_new_.ReadTotalTimeoutMultiplier = 1000;
  timeout_new_.WriteTotalTimeoutMultiplier = 0;
  timeout_new_.WriteTotalTimeoutConstant = 0;

  SetCommTimeouts(com_handle_, &timeout_new_);

  GetCommState(com_handle_, &dcb_old_);

  dcb_new_ = dcb_old_;

  dcb_new_.BaudRate = baud_;
  dcb_new_.ByteSize = 8;
  dcb_new_.fParity = 0;
  dcb_new_.Parity = 0;
  dcb_new_.StopBits = 0;

  SetCommState(com_handle_, &dcb_new_);
}


SerialPortWrapper::~SerialPortWrapper(void) {
  close();
}


void SerialPortWrapper::close() {
  if (com_handle_ == INVALID_HANDLE_VALUE) {
    return;
  }

  SetCommTimeouts(com_handle_, &timeout_old_);
  SetCommState(com_handle_, &dcb_old_);

  CloseHandle(com_handle_);

  com_handle_ = INVALID_HANDLE_VALUE;
}

int SerialPortWrapper::write(const uint8_t* data, size_t sz) {
  if (com_handle_ == INVALID_HANDLE_VALUE) {
    return -1;
  }
  unsigned long count = 0;
  WriteFile(com_handle_, data, sz, &count, NULL);
  return count;
}

int SerialPortWrapper::read(uint8_t* data, size_t sz) {
  if (com_handle_ == INVALID_HANDLE_VALUE) {
    return -1;
  }
  unsigned long count = 0;
  ReadFile(com_handle_, data, sz, &count, NULL);
  return count;
}

bool SerialPortWrapper::operator()() const {
  return com_handle_ != INVALID_HANDLE_VALUE;
}

void SerialPortWrapper::flush() {
  if (com_handle_ == INVALID_HANDLE_VALUE) {
    return;
  }
  FlushFileBuffers(com_handle_);
}

char SerialPortWrapper::get_char() {
  if (com_handle_ == INVALID_HANDLE_VALUE) {
    return -1;
  }
  unsigned long count;
  char c;
  ReadFile(com_handle_, &c, 1, &count, NULL);

  if (count == 1) {
    return c;
  }


  return -1;
}

void SerialPortWrapper::put_char(char C) {
  if (com_handle_ == INVALID_HANDLE_VALUE) {
    return;
  }

  unsigned long count;

  WriteFile(com_handle_, &C, 1, &count, NULL);
}
