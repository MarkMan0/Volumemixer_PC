#include "VolumeAPI/VolumeAPI.h"
#include "SerialPortWrapper/SerialPortWrapper.h"
#include <thread>
#include <atomic>
#include <iostream>
#include <chrono>
#include <variant>
#include <locale>
#include <codecvt>

#include <string_view>

static void serial_comm(SerialPortWrapper&);
static void respond_load(SerialPortWrapper&);
static void respond_img(SerialPortWrapper&);

int main() {
  const int port_id = 8;

  VolumeControl::init();

  std::atomic_bool runflag = true;

  std::thread ui_thread([&runflag]() -> void {
    while (runflag) {
      char q = 0;
      std::cin >> q;
      if (q == 'q' || q == 'e') {
        runflag = false;
      }
    }
  });

  while (runflag) {
    SerialPortWrapper port(8, 115200);
    port.open();
    using namespace std::chrono_literals;
    auto sleep_time = 5s;
    if (port()) {
      sleep_time = 1s;
      serial_comm(port);
      port.close();
    } else {
      std::cout << "NoPort\n";
    }
    std::this_thread::sleep_for(sleep_time);
  }

  ui_thread.join();
}

using all_numeric = std::variant<uint8_t, uint16_t, uint32_t, uint64_t, int8_t, int16_t, int32_t, int64_t, char>;

uint32_t crc32mpeg2(const uint8_t* buf, size_t len, uint32_t crc = 0xffffffff) {
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


template <class T>
void transfer_variant_bytes(const all_numeric& var, std::vector<uint8_t>& out_vec) {
  if (std::holds_alternative<T>(var)) {
    T val = std::get<T>(var);
    auto iter = out_vec.insert(out_vec.end(), sizeof(T), 0);
    uint8_t* dst_address = &(*iter);
    memcpy(dst_address, &val, sizeof(val));
  }
}

void transfer_variant_vec(const std::vector<all_numeric>& in, std::vector<uint8_t>& out) {
  for (const auto& var : in) {
    transfer_variant_bytes<uint8_t>(var, out);
    transfer_variant_bytes<uint16_t>(var, out);
    transfer_variant_bytes<uint32_t>(var, out);
    transfer_variant_bytes<uint64_t>(var, out);
    transfer_variant_bytes<int8_t>(var, out);
    transfer_variant_bytes<int16_t>(var, out);
    transfer_variant_bytes<int32_t>(var, out);
    transfer_variant_bytes<int64_t>(var, out);
    transfer_variant_bytes<char>(var, out);
  }
}


static void serial_comm(SerialPortWrapper& port) {
  std::this_thread::sleep_for(std::chrono::milliseconds(2000));
  char c = port.get_char();

  std::cout << "Got from serial: " << static_cast<int>(c) << '\n';

  switch (c) {
    case -1: {
      std::this_thread::sleep_for(std::chrono::milliseconds(200));
      break;
    }

    case 1: {
      std::cout << "Responding to 0x01\n";
      respond_load(port);
      break;
    }

    case 2: {
      std::cout << "Responding to 0x01\n";
      respond_img(port);
      break;
    }

    default:
      std::cout << "Err: unknown\n";
      break;
  }
}


static void respond_load(SerialPortWrapper& port) {
  namespace VC = VolumeControl;

  const auto sessions = VC::get_all_sessions_info();
  std::vector<all_numeric> vec;
  {
    uint8_t sz = sessions.size();
    uint32_t crc = crc32mpeg2(&sz, 1);
    vec.push_back(sz);
    vec.push_back(crc);
  }

  for (const auto& session : sessions) {
    {
      std::wcout << session << '\n';
      uint32_t pid = session.pid_;
      uint8_t vol = session.volume_;
      uint8_t name_sz = session.filename_.size() + 1;

      uint32_t crc = crc32mpeg2(reinterpret_cast<uint8_t*>(&pid), 4);
      crc = crc32mpeg2(&vol, 1, crc);
      crc = crc32mpeg2(&name_sz, 1, crc);

      vec.push_back(pid);
      vec.push_back(vol);
      vec.push_back(name_sz);
      vec.push_back(crc);
    }
    {
      const std::string s = std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t>().to_bytes(session.filename_);
      const char* cstr = s.c_str();
      uint32_t crc = crc32mpeg2(reinterpret_cast<const uint8_t*>(cstr), s.size() + 1);

      for (char c : s) {
        vec.push_back(c);
      }
      vec.push_back('\0');

      vec.push_back(crc);
    }
  }
  std::vector<uint8_t> out_vec;
  transfer_variant_vec(vec, out_vec);

  port.write(out_vec.data(), out_vec.size());
}

static void respond_img(SerialPortWrapper& port) {
  std::cout << ">>>> Respond img\n";
  namespace VC = VolumeControl;
  const auto sessions = VC::get_all_sessions_info();

  std::vector<all_numeric> vec;

  int pid = 0;

  port.read(reinterpret_cast<uint8_t*>(&pid), sizeof(pid));
  std::cout << "\t PID: " << pid << '\n';
  VC::AudioSessionInfo info;
  bool found = false;
  for (const auto& sess : sessions) {
    if (sess.pid_ == pid) {
      found = true;
      info = sess;
    }
  }
  if (not found) {
    return;
  }

  // send size of image
  const auto png_data = info.get_icon_data();
  uint32_t png_sz = png_data.size();
  vec.push_back(png_sz);
  vec.push_back(crc32mpeg2(reinterpret_cast<uint8_t*>(&png_sz), sizeof(png_sz)));

  std::vector<uint8_t> out_vec;
  transfer_variant_vec(vec, out_vec);

  port.write(out_vec.data(), out_vec.size());
  vec.clear();
  out_vec.clear();

  uint32_t max_chunk_size = 0;
  int read = 0;
  while (read < 4) {
    read = port.read(reinterpret_cast<uint8_t*>(&max_chunk_size) + read, sizeof(max_chunk_size));
  }


  for (uint32_t bytes_written = 0; bytes_written < png_sz;) {
    uint32_t chunk_size = std::min(max_chunk_size, static_cast<uint32_t>(png_data.size() - bytes_written));
    auto written = port.write(reinterpret_cast<const uint8_t*>(png_data.data()) + bytes_written, chunk_size);

    uint32_t crc = crc32mpeg2(reinterpret_cast<const uint8_t*>(png_data.data()) + bytes_written, chunk_size);
    port.write(reinterpret_cast<uint8_t*>(&crc), 4);

    bytes_written += written;

    using namespace std::chrono_literals;
    while (port.get_char() == -1) {
      std::cout << "Processing chunk\n";
    std::this_thread::sleep_for(10ms);
    }
  }
}
