#include "communication.h"
#include "VolumeAPI/VolumeAPI.h"
#include "CommSupervisor/supervisor.h"
#include <chrono>
#include <string>
#include <thread>
#include <codecvt>
#include <array>


static uint32_t glob_last_crc = 0;
static uint32_t compute_session_checksum(const std::vector<VolumeControl::AudioSessionInfo>&);



template <class T>
static inline T mem2T(const uint8_t* mem) {
  return *reinterpret_cast<const T*>(mem);
}


bool serial_comm(SerialPortWrapper& port) {
  const auto buffer = wait_data(port, 1);
  if (buffer.size() < 1) {
    glob_last_crc = 0;
    DEBUG_PRINT("No data\n");
    return false;
  }

  const uint8_t c = mem2T<uint8_t>(buffer.data());

  switch (c) {
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


void respond_load(SerialPortWrapper& port) {
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

  auto data = wait_data(port, 5);
  if (data.size() >= 5 && CRC::verify_crc(data.data(), 5) && data[0] == mixer::commands::RESPONSE_OK) {
    DEBUG_PRINT("\tsend success\n");
    glob_last_crc = compute_session_checksum(sessions);
  } else {
    DEBUG_PRINT("\tsend failure\n");
    glob_last_crc = 0;
  }
}

void respond_img(SerialPortWrapper& port) {
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

    auto data = wait_data(port, 5);
    if (data.size() >= 5 && CRC::verify_crc(data.data(), 5) && data[0] == mixer::commands::RESPONSE_OK) {
    } else {
      DEBUG_PRINT("\tchunk fail");
      return;
    }
  }
  DEBUG_PRINT("\tSend IMG success\n");
}



std::vector<uint8_t> wait_data(SerialPortWrapper& port, size_t n) {
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


void respond_set_volume(SerialPortWrapper& port) {
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

void respond_echo(SerialPortWrapper& port) {
  uint8_t c = '\0';
  while (port.read(&c, 1) && c != '\0') {
    std::cout << static_cast<char>(c);
  }
}

void respond_mute(SerialPortWrapper& port) {
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



void respond_query_changes(SerialPortWrapper& port) {
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