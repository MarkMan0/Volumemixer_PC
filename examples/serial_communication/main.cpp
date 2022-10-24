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

  Supervisor sv;


  const auto sessions = VC::get_all_sessions_info();

  sv.append(static_cast<uint8_t>(sessions.size()));
  sv.compute_crc();

  for (const auto& session : sessions) {
    std::wcout << session << '\n';

    sv.append(static_cast<uint32_t>(session.pid_));
    sv.append(static_cast<uint8_t>(session.volume_));
    sv.append(static_cast<uint8_t>(session.filename_.size() + 1));
    sv.compute_crc();

    const std::string s = std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t>().to_bytes(session.filename_);
    for (char c : s) {
      sv.append(c);
    }
    sv.append('\0');
    sv.compute_crc();
  }

  port.write(sv.get_buffer().data(), sv.get_buffer().size());
}

static void respond_img(SerialPortWrapper& port) {
/*  std::cout << ">>>> Respond img\n";
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
  */
}
