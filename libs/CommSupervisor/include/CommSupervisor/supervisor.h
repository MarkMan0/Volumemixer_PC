#pragma once
#include <vector>



class Supervisor {
public:
  /// @brief compute the CRC for the buffer
  /// @param buffer pointer to memory
  /// @param len length of @p buffer in bytes
  /// @param crc the initial CRC value
  /// @return the computed CRC
  static uint32_t crc32mpeg2(const void* buffer, size_t len, uint32_t crc = 0xFFFFFFFF);

  /// @brief verify the crc for buffer, crc is not in the buffer
  /// @param buffer pointer to buffer
  /// @param len length of buffer
  /// @param crc_in expected CRC
  /// @return true if CRC matches expected
  static bool verify_crc(const void* buffer, size_t len, uint32_t crc_in);

  /// @brief verify buffer, when CRC is the last 4 bytes in buffer
  /// @param buffer pointer to memory
  /// @param len length of buffer, including CRC
  /// @return true if buffer is correct
  static bool verify_buffer(const void* buffer, size_t len);

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

  void compute_crc();

private:
  void append_any(const void* mem, size_t sz);
  std::vector<uint8_t> buffer_;
  size_t last_crc_ = 0;
};