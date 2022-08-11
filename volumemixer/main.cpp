#include <iostream>
#include "VolumeAPI.h"
#include <fstream>
#include <string>


int main() {
  using namespace VolumeControl;

  if (not init()) {
    return 1;
  }

  const auto ret = get_all_sessions_info();

  int cnt = 0;
  for (auto i : ret) {
    std::wcout << i << '\n';
    if (i.icon_data_.size()) {
      std::ofstream file(L"./" + std::to_wstring(cnt++) + L".ico", std::ios::out | std::ios::binary);
      file.write(i.icon_data_.data(), i.icon_data_.size());
    }
  }

  std::wcout << get_master_info() << '\n';

  return 0;
}