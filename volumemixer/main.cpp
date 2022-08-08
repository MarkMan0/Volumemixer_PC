#include <iostream>
#include "VolumeAPI.h"


int main() {

  using namespace VolumeControl;

  if (not init()) {
    return 1;
  }

  const auto pid = get_all_pid();

  for (auto p : pid) {
    std::cout << "PID: " << p << "\t Volume: " << get_app_volume(p) << '\n';
  }

  std::cout << "Master: " << get_master_volume() << '\n';

  return 0;
} 