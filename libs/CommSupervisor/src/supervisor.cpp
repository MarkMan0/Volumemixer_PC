#include "CommSupervisor/supervisor.h"


uint32_t CRC::crc32mpeg2(const void* buffer, size_t len, uint32_t crc) {
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

bool CRC::verify_crc(const void* buffer, size_t len, uint32_t crc_in) {
  uint32_t crc = crc32mpeg2(buffer, len);
  return crc == crc_in;
}

bool CRC::verify_crc(const void* buffer, size_t len) {
  if (len < 5) {
    return false;
  }
  uint32_t crc = crc32mpeg2(buffer, len - 4);
  uint32_t crc_exp = *reinterpret_cast<const uint32_t*>(reinterpret_cast<const uint8_t*>(buffer) + len - 4);

  return crc == crc_exp;
}

void Hasher::begin_message() {
  buffer_.clear();
  last_crc_ = 0;
}


void Hasher::compute_crc() {
  auto start_iter = buffer_.cbegin() + last_crc_;

  const uint8_t* start_ptr = &(*start_iter);
  uint32_t sz = buffer_.cend() - start_iter;

  uint32_t crc = CRC::crc32mpeg2(start_ptr, sz);
  append(crc);
  last_crc_ += sz;
  last_crc_ += 4;
}

void Hasher::append_any(const void* ptr, size_t sz) {
  const uint8_t* buff = reinterpret_cast<const uint8_t*>(ptr);
  buffer_.reserve(sz);
  for (size_t i = 0; i < sz; ++i) {
    buffer_.push_back(buff[i]);
  }
}


void DeHasher::reset() {
  buffer_.clear();
}


void DeHasher::append_any(const void* buff, size_t sz) {
  const uint8_t* bytes = reinterpret_cast<const uint8_t*>(buff);
  for (size_t i = 0; i < sz; ++i) {
    buffer_.push_back(bytes[i]);
  }
}


bool DeHasher::verify_crc() {
  return CRC::verify_crc(buffer_.data(), buffer_.size());
}

bool DeHasher::verify_crc(uint32_t crc_in) {
  return CRC::verify_crc(buffer_.data(), buffer_.size(), crc_in);
}
