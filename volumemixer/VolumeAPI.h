#pragma once

#include <vector>
#include <string>
#include <ostream>

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


  struct AudioSessionInfo {
    std::wstring path_;
    std::wstring icon_path_;
    int pid_;
    float volume_;
    bool muted_;
  };

  [[nodiscard]] std::vector<AudioSessionInfo> get_all_sessions_info();
  [[nodiscard]] AudioSessionInfo get_master_info();
};  // namespace VolumeControl


inline std::wostream& operator<<(std::wostream& out, const VolumeControl::AudioSessionInfo& audio) {
  out << audio.path_ << std::wstring(L"\n\tvolume: ") << std::to_wstring(audio.volume_) << std::wstring( L"\n\tmuted: ") << std::to_wstring(audio.muted_)
      << std::wstring(L"\n\ticon: ") << audio.icon_path_ << std::wstring(L"\n\tPID: ") << std::to_wstring(audio.pid_);

  return out;
}


namespace ProcessAPI {

  std::wstring get_path_from_pid(int pid);
  std::vector<std::wstring> get_icon_from_pid(int pid);

};