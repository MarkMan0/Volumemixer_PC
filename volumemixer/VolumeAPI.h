#pragma once

#include <vector>

namespace VolumeControl {

  [[nodiscard]] bool init();

  [[nodiscard]] float get_master_volume();
  void set_master_volume(float level);

  [[nodiscard]] bool get_master_mute();
  void set_master_mute(bool mute);

  [[nodiscard]] float get_app_volume(int pid);
  void set_app_volume(int pid, float level);

  [[nodiscard]] bool get_app_mute(int pid);
  void set_app_mute(int pid, bool mute);

  [[nodiscard]] std::vector<int> get_all_pid();


};