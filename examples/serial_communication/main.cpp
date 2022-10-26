#include "VolumeAPI/VolumeAPI.h"
#include "SerialPortWrapper/SerialPortWrapper.h"
#include <thread>
#include <atomic>
#include <iostream>
#include <chrono>
#include <variant>
#include <locale>
#include <codecvt>
#include "CommSupervisor/supervisor.h"
#include <algorithm>

#include <string_view>

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

#define DO_DEBUG
#ifdef DO_DEBUG
  #define DEBUG_PRINT(ARG)  std::cout << ARG;
  #define DEBUG_WPRINT(ARG) std::wcout << ARG;
#else
  #define DEBUG_PRINT(ARG)
  #define DEBUG_WPRINT(ARG)
#endif


template <class T>
inline T mem2T(const uint8_t* mem) {
  return *reinterpret_cast<const T*>(mem);
}

static bool serial_comm(SerialPortWrapper&);
static void respond_load(SerialPortWrapper&);
static void respond_img(SerialPortWrapper&);
static void respond_set_volume(SerialPortWrapper&);
static void respond_echo(SerialPortWrapper&);
static void respond_mute(SerialPortWrapper&);
static void respond_query_changes(SerialPortWrapper&);
static std::vector<uint8_t> wait_data(SerialPortWrapper&, size_t);
static uint32_t compute_session_checksum(const std::vector<VolumeControl::AudioSessionInfo>&);

static uint32_t glob_last_crc = 0;

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

  DEBUG_PRINT("Entering main loop\n");
  while (runflag) {
    DEBUG_PRINT("While loop...\n");
    SerialPortWrapper port(8, 115200);
    port.open();
    using namespace std::chrono_literals;
    auto sleep_time = 5s;
    if (port()) {
      DEBUG_PRINT("Port is open\n");
      sleep_time = 1s;
      bool b = true;
      while (b) {
        DEBUG_PRINT("Entering data handler\n");
        b = serial_comm(port);
      }
      DEBUG_PRINT("Port timed out, closing\n");
      port.close();
    } else {
      DEBUG_PRINT("No port\n");
    }
    DEBUG_PRINT("Sleeping\n");
    std::this_thread::sleep_for(sleep_time);
  }

  ui_thread.join();
}


static bool serial_comm(SerialPortWrapper& port) {
  const auto buffer = wait_data(port, 1);
  if (buffer.size() < 1) {
    DEBUG_PRINT("No data\n");
    return false;
  }

  const uint8_t c = mem2T<uint8_t>(buffer.data());
  
  switch (c) {
    case -1: {
      std::this_thread::sleep_for(std::chrono::milliseconds(200));
      break;
    }

    case mixer::commands::LOAD_ALL: {
      DEBUG_PRINT("respond_load()\n");
      respond_load(port);
      DEBUG_PRINT("respond_load() DONE\n");
      break;
    }

    case mixer::commands::READ_IMG: {
      DEBUG_PRINT("respond_img()\n");
      respond_img(port);
      DEBUG_PRINT("respond_img() DONE\n");
      break;
    }

    case mixer::commands::SET_VOLUME: {
      DEBUG_PRINT("respond_set_volume()\n");
      respond_set_volume(port);
      DEBUG_PRINT("respond_set_volume() DONE\n");
      break;
    }

    case mixer::commands::ECHO: {
      DEBUG_PRINT("respond_echo()\n");
      respond_echo(port);
      DEBUG_PRINT("respond_echo() DONE\n");
      break;
    }

    case mixer::commands::SET_MUTE: {
      DEBUG_PRINT("respond_mute()\n");
      respond_mute(port);
      DEBUG_PRINT("respond_mute() DONE\n");
      break;
    }

    case mixer::commands::QUERY_CHANGES: {
      DEBUG_PRINT("respond_query_changes()\n");
      respond_query_changes(port);
      DEBUG_PRINT("respond_query_changes() DONE\n");
      break;
    }

    default:
      DEBUG_PRINT("Err: unknown " << static_cast<int>(c) << "\n");
      break;
  }

  return true;
}


static void respond_load(SerialPortWrapper& port) {
  namespace VC = VolumeControl;

  Hasher sv;
  auto sessions = VC::get_all_sessions_info();


  sv.append(static_cast<uint8_t>(sessions.size()));
  sv.compute_crc();

  for (const auto& session : sessions) {
    // std::wcout << session << '\n';

    sv.append(static_cast<int16_t>(session.pid_));
    sv.append(static_cast<uint8_t>(session.volume_));
    sv.append(static_cast<uint8_t>(session.muted_));
    sv.append(static_cast<uint8_t>(session.filename_.size() + 1));
    sv.compute_crc();

    const std::string s = std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t>().to_bytes(session.filename_);
    for (char c : s) {
      sv.append(c);
    }
    sv.append('\0');
    sv.compute_crc();
  }
  DEBUG_PRINT("\t data length: " << sv.get_buffer().size() << '\n');
  port.write(sv.get_buffer().data(), sv.get_buffer().size());

  auto data = wait_data(port, 2);
  if (data.size() >= 2 && data[0] == mixer::commands::RESPONSE_OK_0 && data[1] == mixer::commands::RESPONSE_OK_1) {
    DEBUG_PRINT("\tsend success\n");
    glob_last_crc = compute_session_checksum(sessions);
  } else {
    DEBUG_PRINT("\tsend failure\n");
    glob_last_crc = 0;
  }
}

static void respond_img(SerialPortWrapper& port) {
  namespace VC = VolumeControl;

  const auto sessions = VC::get_all_sessions_info();
  Hasher hasher;
  DeHasher dehasher;


  const auto pid_data = wait_data(port, sizeof(int16_t) + 4);
  if (pid_data.size() < sizeof(int16_t) + 4) {
    DEBUG_PRINT("\t No PID\n");
    return;
  }
  if (not CRC::verify_crc(pid_data.data(), 6)) {
    DEBUG_PRINT("\tPID CRC Error\n");
    return;
  }
  const int16_t pid = mem2T<int16_t>(pid_data.data());

  DEBUG_PRINT("\tPID: " << pid << '\n');

  VC::AudioSessionInfo info;
  bool found = false;
  for (const auto& sess : sessions) {
    if (sess.pid_ == pid) {
      found = true;
      info = sess;
      break;
    }
  }
  if (not found) {
    DEBUG_PRINT("\t session not found\n");
    return;
  }
  DEBUG_WPRINT("\tSession: " << info.filename_ << '\n');

  // send size of image
  const auto png_data = info.get_icon_data();
  uint32_t png_sz = png_data.size();
  DEBUG_PRINT("\tPNG size: " << png_sz);
  hasher.append(png_sz);
  hasher.compute_crc();
  DEBUG_PRINT("\tWriting bytes: " << hasher.get_buffer().size() << '\n');
  port.write(hasher.get_buffer().data(), hasher.get_buffer().size());
  hasher.begin_message();


  const auto chunk_data = wait_data(port, sizeof(uint32_t) + 4);
  if (chunk_data.size() < sizeof(uint32_t) + 4) {
    DEBUG_PRINT("\t No chunk_data\n");
    return;
  }
  if (not CRC::verify_crc(chunk_data.data(), 8)) {
    DEBUG_PRINT("\tChunk CRC error\n");
    return;
  }
  const uint32_t max_chunk_size = mem2T<uint32_t>(chunk_data.data());
  DEBUG_PRINT("\tChunk size is: " << max_chunk_size << '\n');


  for (uint32_t bytes_written = 0; bytes_written < png_sz;) {
    uint32_t chunk_size = std::min(max_chunk_size, static_cast<uint32_t>(png_data.size() - bytes_written));

    auto written = port.write(reinterpret_cast<const uint8_t*>(png_data.data()) + bytes_written, chunk_size);
    uint32_t crc = CRC::crc32mpeg2(png_data.data() + bytes_written, chunk_size);

    port.write(reinterpret_cast<uint8_t*>(&crc), 4);

    bytes_written += written;

    if (wait_data(port, 1).size() < 1) {
      DEBUG_PRINT("\tChunk timeout\n");
      return;
    }
  }
  DEBUG_PRINT("\tSend IMG success\n");
}



static std::vector<uint8_t> wait_data(SerialPortWrapper& port, size_t n) {
  std::vector<uint8_t> buff;
  using namespace std::chrono_literals;

  auto start = std::chrono::steady_clock::now();
  buff.reserve(n);

  while (buff.size() < n) {
    uint8_t b;
    if (port.read(&b, 1)) {
      buff.push_back(b);
    }

    if ((std::chrono::steady_clock::now() - start) > 10s) {
      DEBUG_PRINT("Serial timeout\n");
      return {};
    }
  }

  return buff;
}


static void respond_set_volume(SerialPortWrapper& port) {
  constexpr size_t expected_length = sizeof(int16_t) + sizeof(uint8_t) + 4;
  const auto vol_data = wait_data(port, expected_length);
  if (vol_data.size() < expected_length) {
    DEBUG_PRINT("\t No data\n");
    return;
  }

  if (not CRC::verify_crc(vol_data.data(), expected_length)) {
    DEBUG_PRINT("\tCRC error\n");
    return;
  }

  const int16_t pid = mem2T<int16_t>(vol_data.data());
  const uint8_t vol = mem2T<uint8_t>(vol_data.data() + 2);

  VolumeControl::set_volume(pid, vol);

  DEBUG_PRINT("\tDone\n");
}

static void respond_echo(SerialPortWrapper& port) {
  uint8_t c = '\0';
  while (port.read(&c, 1)) {
    std::cout << static_cast<char>(c);
  }
}

static void respond_mute(SerialPortWrapper& port) {
  const auto mute_data = wait_data(port, 2 + 1 + 4);  // pid, muted, crc
  if (mute_data.size() < 2 + 1 + 4) {
    DEBUG_PRINT("\t No data\n");
    return;
  }
  if (not CRC::verify_crc(mute_data.data(), 2 + 1 + 4)) {
    DEBUG_PRINT("\tCRC error\n");
    return;
  }

  const int16_t pid = mem2T<int16_t>(mute_data.data());
  const bool mute = mem2T<uint8_t>(mute_data.data() + 2);

  VolumeControl::set_muted(pid, mute);
}



static void respond_query_changes(SerialPortWrapper& port) {
  bool changed = glob_last_crc != compute_session_checksum(VolumeControl::get_all_sessions_info());
  Hasher crc;
  DEBUG_PRINT("\tchange: " << changed << '\n');
  crc.append(static_cast<uint8_t>(changed));
  crc.compute_crc();
  port.write(crc.get_buffer().data(), crc.get_buffer().size());
}


static uint32_t compute_session_checksum(const std::vector<VolumeControl::AudioSessionInfo>& sessions) {
  Hasher crc;
  for (const auto& session : sessions) {
    crc.append(session.pid_);
    crc.append(session.volume_);
    crc.append(session.muted_);
    const wchar_t* c_str = session.filename_.c_str();
    crc.append_buff(c_str, sizeof(wchar_t) * (1 + session.filename_.size()));
  }

  return crc.compute_crc();
}