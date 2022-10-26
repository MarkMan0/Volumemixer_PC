#include "VolumeAPI/VolumeAPI.h"
#include <functional>
#include <windows.h>
#include <mmdeviceapi.h>
#include <audiopolicy.h>
#include <endpointvolume.h>
#include <Psapi.h>
#include <filesystem>
#include <Windows.h>
#include <shellapi.h>
#include "process_api.h"

namespace fs = std::filesystem;



// Based on: https://github.com/chrispader/VolumeControl

template <class T>
void SAFE_RELEASE(T*& x) {
  if (x) {
    x->Release();
    x = NULL;
  }
}


/// @brief get the audio master volume
static IAudioEndpointVolume* GetMaster() {
  HRESULT hr;
  IMMDeviceEnumerator* enumerator = NULL;
  IMMDevice* device = NULL;
  IAudioEndpointVolume* endpoint = NULL;

  hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_INPROC_SERVER, __uuidof(IMMDeviceEnumerator),
                        (void**)&enumerator);

  hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);

  hr = device->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_INPROC_SERVER, NULL, (void**)&endpoint);

  return endpoint;
}

/// @brief Calls @p callback for each session. If return of @p callback is true, stops and returns to caller
/// @param callback std::function object
static void session_enumerate(std::function<bool(IAudioSessionControl*, IAudioSessionControl2*, DWORD)> callback) {
  IMMDeviceEnumerator* enumerator = NULL;
  ISimpleAudioVolume* volume = NULL;
  IMMDevice* device = NULL;
  IAudioSessionManager2* manager = NULL;
  IAudioSessionEnumerator* sessionEnumerator = NULL;

  if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
                              (void**)&enumerator))) {
    return;
  }
  if (FAILED(enumerator->GetDefaultAudioEndpoint(EDataFlow::eRender, ERole::eMultimedia, &device))) {
    return;
  }
  if (FAILED((device->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, NULL, (void**)&manager)))) {
    return;
  }
  if (FAILED((manager->GetSessionEnumerator(&sessionEnumerator)))) {
    return;
  }

  // Get the session count
  int sessionCount = 0;
  if (FAILED(sessionEnumerator->GetCount(&sessionCount))) {
    return;
  }

  // Loop through all sessions
  for (int i = 0; i < sessionCount; i++) {
    IAudioSessionControl* ctrl = NULL;
    IAudioSessionControl2* ctrl2 = NULL;
    DWORD processId = 0;
    if (FAILED(sessionEnumerator->GetSession(i, &ctrl))) {
      continue;
    }
    if (FAILED(ctrl->QueryInterface(__uuidof(IAudioSessionControl2), (void**)&ctrl2))) {
      SAFE_RELEASE(ctrl);
      continue;
    }
    DWORD pid;
    if (FAILED(ctrl2->GetProcessId(&pid))) {
      SAFE_RELEASE(ctrl);
      SAFE_RELEASE(ctrl2);
      continue;
    }

    const bool ret = callback(ctrl, ctrl2, pid);

    SAFE_RELEASE(ctrl);
    SAFE_RELEASE(ctrl2);

    if (ret) {
      break;
    }
  }
}

/// @brief Get volume session for @p pid
static ISimpleAudioVolume* GetSession(int pid) {
  ISimpleAudioVolume* session;

  // enumerate through all sessions, if PID matches load into session* and stop enumerating
  auto cb = [&session, pid](IAudioSessionControl* ctrl, IAudioSessionControl2* ctrl2, DWORD curr_pid) {
    if (pid == curr_pid) {
      if (FAILED(ctrl2->QueryInterface(__uuidof(ISimpleAudioVolume), (void**)&session))) {
        session = NULL;
      }
      return true;
    }
    return false;
  };

  session_enumerate(cb);
  return session;
}


static float get_master_volume() {
  IAudioEndpointVolume* endpoint = GetMaster();
  if (endpoint == NULL) return NULL;
  float level;
  endpoint->GetMasterVolumeLevelScalar(&level);
  return level * 100;
}

static bool get_master_mute() {
  IAudioEndpointVolume* endpoint = GetMaster();
  if (endpoint == NULL) return NULL;
  BOOL mute;
  endpoint->GetMute(&mute);
  return mute;
}

static void set_master_volume(float level) {
  IAudioEndpointVolume* endpoint = GetMaster();
  if (endpoint == NULL) return;
  endpoint->SetMasterVolumeLevelScalar(level / 100, NULL);
  SAFE_RELEASE(endpoint);
}

static void set_master_mute(bool mute) {
  IAudioEndpointVolume* endpoint = GetMaster();
  if (endpoint == NULL) return;
  endpoint->SetMute(mute, NULL);
  SAFE_RELEASE(endpoint);
}


static VolumeControl::AudioSessionInfo get_master_info() {
  VolumeControl::AudioSessionInfo info;
  info.filename_ = L"Master";
  info.pid_ = -1;
  info.volume_ = get_master_volume();
  info.muted_ = get_master_mute();

  return info;
}


static float get_app_volume(int pid) {
  ISimpleAudioVolume* volume = GetSession(pid);
  if (volume == NULL) return NULL;
  float level = NULL;
  volume->GetMasterVolume(&level);
  SAFE_RELEASE(volume);
  return level * 100;
}

static bool get_app_mute(int pid) {
  ISimpleAudioVolume* volume = GetSession(pid);
  if (volume == NULL) return NULL;
  BOOL mute = NULL;
  volume->GetMute(&mute);
  SAFE_RELEASE(volume);
  return mute;
}

static void set_app_volume(int pid, float level) {
  ISimpleAudioVolume* volume = GetSession(pid);
  if (volume == NULL) return;
  volume->SetMasterVolume(level / 100, NULL);
  SAFE_RELEASE(volume);
}

static void set_app_mute(int pid, bool mute) {
  ISimpleAudioVolume* volume = GetSession(pid);
  if (volume == NULL) return;
  volume->SetMute(mute, NULL);
  SAFE_RELEASE(volume);
}


//////
////// PUBLIC API
//////


std::wostream& operator<<(std::wostream& out, const VolumeControl::AudioSessionInfo& audio) {
  out << audio.filename_ << std::wstring(L"\n\tpath: ") << audio.path_ << std::wstring(L"\n\tvolume: ")
      << std::to_wstring(audio.volume_) << std::wstring(L"\n\tmuted: ") << std::to_wstring(audio.muted_)
      << std::wstring(L"\n\ticon: ") << audio.get_icon_data().size() << std::wstring(L"\n\tPID: ")
      << std::to_wstring(audio.pid_);

  return out;
}

std::vector<uint8_t> VolumeControl::AudioSessionInfo::get_icon_data() const {
  return ProcessAPI::get_png_from_pid(pid_);
}

bool VolumeControl::init() {
  return SUCCEEDED(CoInitialize(NULL));
}

std::vector<VolumeControl::AudioSessionInfo> VolumeControl::get_all_sessions_info() {
  std::vector<AudioSessionInfo> ret;

  auto cb = [&ret](IAudioSessionControl* ctrl, IAudioSessionControl2* ctrl2, DWORD pid) {
    AudioSessionInfo info{};
    info.pid_ = pid;
    ISimpleAudioVolume* volume;
    if (SUCCEEDED(ctrl2->QueryInterface(__uuidof(ISimpleAudioVolume), (void**)&volume))) {
      float vol = 0;
      volume->GetMasterVolume(&vol);
      info.volume_ = vol * 100.0f;
      BOOL b;
      volume->GetMute(&b);
      info.muted_ = b;
      SAFE_RELEASE(volume);
    }

    if (pid == 0) {
      info.filename_ = L"System";
    } else {
      info.path_ = ProcessAPI::get_path_from_pid(pid);
      info.filename_ = fs::path(info.path_).filename().stem().wstring();
    }
    ret.push_back(info);
    return false;
  };

  session_enumerate(cb);
  ret.push_back(get_master_info());

  std::sort(ret.begin(), ret.end(),
            [](const AudioSessionInfo& l, const AudioSessionInfo& r) { return l.pid_ < r.pid_; });
  return ret;
}

float VolumeControl::get_volume(int pid) {
  if (pid == -1) {
    return get_master_volume();
  } else {
    return get_app_volume(pid);
  }
}

void VolumeControl::set_volume(int pid, float volume) {
  if (pid == -1) {
    set_master_volume(volume);
  } else {
    set_app_volume(pid, volume);
  }
}

bool VolumeControl::get_muted(int pid) {
  if (pid == -1) {
    return get_master_mute();
  } else {
    return get_app_mute(pid);
  }
}

void VolumeControl::set_muted(int pid, bool mute) {
  if (pid == -1) {
    set_master_mute(mute);
  } else {
    set_app_mute(pid, mute);
  }
}