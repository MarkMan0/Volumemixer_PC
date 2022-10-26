#pragma once
#include <vector>


namespace CRC {
  /// @brief compute the CRC for the buffer
  /// @param buffer pointer to memory
  /// @param len length of @p buffer in bytes
  /// @param crc the initial CRC value
  /// @return the computed CRC
  uint32_t crc32mpeg2(const void* buffer, size_t len, uint32_t crc = 0xFFFFFFFF);

  /// @brief verify the crc for buffer, crc is not in the buffer
  /// @param buffer pointer to buffer
  /// @param len length of buffer
  /// @param crc_in expected CRC
  /// @return true if CRC matches expected
  bool verify_crc(const void* buffer, size_t len, uint32_t crc_in);

  /// @brief verify buffer, when CRC is the last 4 bytes in buffer
  /// @param buffer pointer to memory
  /// @param len length of buffer, including CRC
  /// @return true if buffer is correct
  bool verify_crc(const void* buffer, size_t len);

};  // namespace CRC

class Hasher {
public:
  /// @brief Empty the buffer and start appending to it
  void begin_message();

  template <class T>
  void append(const T& val) {
    append_any(&val, sizeof(T));
  }

  template <class T>
  void append_buff(const T* mem, size_t sz) {
    append_any(static_cast<const void*>(mem), sz * sizeof(T));
  }

  const std::vector<uint8_t>& get_buffer() {
    return buffer_;
  }

  uint32_t compute_crc();

private:
  void append_any(const void* mem, size_t sz);
  std::vector<uint8_t> buffer_;
  size_t last_crc_ = 0;
};


class DeHasher {
public:
  void reset();

  template <class T>
  void append(const T& data) {
    append_any(&data, sizeof(T));
  }

  template <class T>
  void append(const T* buff, size_t sz) {
    append_any(buff, sz);
  }

  /// @brief verify crc at the end of buffer
  /// @return true if crc is correct
  bool verify_crc();

  /// @brief verify crc of whole buffer aginst @p crc
  /// @param crc expected crc
  /// @return true if matches
  bool verify_crc(uint32_t crc);

private:
  void append_any(const void* buff, size_t sz);
  std::vector<uint8_t> buffer_;
};