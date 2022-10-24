#pragma once
#include <string>
#include <vector>


namespace ProcessAPI {

  [[nodiscard]] std::wstring get_path_from_pid(int pid);
  [[nodiscard]] std::vector<uint8_t> get_png_from_pid(int pid);

};  // namespace ProcessAPI