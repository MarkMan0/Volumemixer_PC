#pragma once

#include <vector>
#include <string>
#include <ostream>

namespace VolumeControl {

  /// @brief Describes an audio session
  struct AudioSessionInfo {
    std::wstring path_;                                        ///< Path of the executable
    std::wstring filename_;                                    ///< name of the executable
    int pid_;                                                  ///< process ID of the executable
    float volume_;                                             ///< volume of session in %
    bool muted_;                                               ///< is the session muted
    [[nodiscard]] std::vector<uint8_t> get_icon_data() const;  ///< load the icon for the executable
  };

  /// @brief Initialize the Winapi
  /// @return true on success
  [[nodiscard]] bool init();

  /// @brief return the info of every active session
  [[nodiscard]] std::vector<AudioSessionInfo> get_all_sessions_info();

  /// @brief Get the volume of the process @p pid.
  /// Set @p pid to -1 to get master volume
  /// @param pid the PID of the process or -1 for master
  /// @return the volume in %
  [[nodiscard]] float get_volume(int pid);


  /// @brief Set the volume of the process @p pid.
  /// Set @p pid to -1 to set master volume
  /// @param pid the PID of the process or -1 for master
  /// @param volume the volume level in %
  void set_volume(int pid, float volume);

  /// @brief return true if @p pid is muted. Pass -1 to get master
  [[nodiscard]] bool get_muted(int pid);

  /// @brief set muted for @p pid, pass -1 to set master
  void set_muted(int pid, bool mute);

};  // namespace VolumeControl


std::wostream& operator<<(std::wostream& out, const VolumeControl::AudioSessionInfo& audio);
