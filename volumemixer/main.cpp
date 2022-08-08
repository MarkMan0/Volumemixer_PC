#include <iostream>
#include "VolumeAPI.h"


int main() {

  using namespace VolumeControl;

  if (not init()) {
    return 1;
  }
  
  const auto ret = get_all_sessions_info();

  for (auto i : ret) {
    std::wcout << i << '\n';
  }

  std::wcout << get_master_info() << '\n';

  return 0;
} 