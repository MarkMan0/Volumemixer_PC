#include "VolumeAPI/VolumeAPI.h"
#include "SerialPortWrapper/SerialPortWrapper.h"
#include <thread>
#include <atomic>
#include <chrono>
#include "communication.h"


int main() {
  const int port_id = 8;

  if (not VolumeControl::init()) {
    return -1;
  }

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
