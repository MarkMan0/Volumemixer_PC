#include "CommSupervisor/supervisor.h"


uint32_t Supervisor::crc32mpeg2(const void* buffer, size_t len, uint32_t crc) {
  const uint8_t* buf = reinterpret_cast<const uint8_t*>(buffer);
  for (unsigned i = 0; i < len; ++i) {
    crc ^= buf[i] << 24;
    for (unsigned j = 0; j < 8; ++j) {
      if ((crc & 0x80000000) == 0) {
        crc = crc << 1;
      } else {
        crc = (crc << 1) ^ 0x104c11db7;
      }
    }
  }
  return crc;
}

bool Supervisor::verify_crc(const void* buffer, size_t len, uint32_t crc_in) {
  uint32_t crc = crc32mpeg2(buffer, len);
  return crc == crc_in;
}

bool Supervisor::verify_buffer(const void* buffer, size_t len) {
  if (len < 5) {
    return false;
  }
  uint32_t crc = crc32mpeg2(buffer, len - 4);
  uint32_t crc_exp = *reinterpret_cast<const uint32_t*>(reinterpret_cast<const uint8_t*>(buffer) + len - 4);

  return crc == crc_exp;
}

void Supervisor::begin_message() {
  buffer_.clear();
}


const std::vector<uint8_t>& Supervisor::end_message() {
  uint32_t crc = crc32mpeg2(buffer_.data(), buffer_.size());
  append(crc);
  return buffer_;
}

void Supervisor::append_any(const void* ptr, size_t sz) {
  const uint8_t* buff = reinterpret_cast<const uint8_t*>(ptr);
  buffer_.reserve(sz);
  for (size_t i = 0; i < sz; ++i) {
    buffer_.push_back(buff[i]);
  }
}