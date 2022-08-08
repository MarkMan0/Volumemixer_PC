#include "VolumeAPI.h"
#include <functional>
#include <windows.h>
#include <mmdeviceapi.h>
#include <audiopolicy.h>
#include <endpointvolume.h>

// Based on: https://github.com/chrispader/VolumeControl

#define SAFE_RELEASE(x)                                                                                                \
  if (x) {                                                                                                             \
    x->Release();                                                                                                      \
    x = NULL;                                                                                                          \
  }


bool wrapped_call(HRESULT hr) {
  return not FAILED(hr);
}

template <class T>
void m_deleter(T* ptr) {
  if (ptr) {
    ptr->Release();
  }
}

namespace VolumeControl {

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

  static void session_enumerate(std::function<bool(IAudioSessionControl*, IAudioSessionControl2*, DWORD)> callback) {
    IMMDeviceEnumerator* enumerator = NULL;
    ISimpleAudioVolume* volume = NULL;
    IMMDevice* device = NULL;
    IAudioSessionManager2* manager = NULL;
    IAudioSessionEnumerator* sessionEnumerator = NULL;

    if (not wrapped_call(CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
                                          (void**)&enumerator))) {
      return;
    }


    if (not wrapped_call(enumerator->GetDefaultAudioEndpoint(EDataFlow::eRender, ERole::eMultimedia, &device))) {
      return;
    }

    if (not wrapped_call((device->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, NULL, (void**)&manager)))) {
      return;
    }

    if (not wrapped_call((manager->GetSessionEnumerator(&sessionEnumerator)))) {
      return;
    }

    // Get the session count
    int sessionCount = 0;
    if (not wrapped_call(sessionEnumerator->GetCount(&sessionCount))) {
      return;
    }

    // Loop through all sessions
    for (int i = 0; i < sessionCount; i++) {
      IAudioSessionControl* ctrl = NULL;
      IAudioSessionControl2* ctrl2 = NULL;
      DWORD processId = 0;
      if (not wrapped_call(sessionEnumerator->GetSession(i, &ctrl))) {
        continue;
      }
      if (not wrapped_call(ctrl->QueryInterface(__uuidof(IAudioSessionControl2), (void**)&ctrl2))) {
        SAFE_RELEASE(ctrl);
        continue;
      }
      DWORD pid;
      if (not wrapped_call(ctrl2->GetProcessId(&pid))) {
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


  static ISimpleAudioVolume* GetSession(int pid) {
    ISimpleAudioVolume* session;

    auto cb = [&session, pid](IAudioSessionControl* ctrl, IAudioSessionControl2* ctrl2, DWORD curr_pid) {
      if (pid == curr_pid) {
        if (not wrapped_call(ctrl2->QueryInterface(__uuidof(ISimpleAudioVolume), (void**)&session))) {
          session = NULL;
        }
        return true;
      }
      return false;
    };

    session_enumerate(cb);
    return session;
  }
}  // namespace VolumeControl


bool VolumeControl::init() {
  return not FAILED(CoInitialize(NULL));
}

float VolumeControl::get_master_volume() {
  IAudioEndpointVolume* endpoint = GetMaster();
  if (endpoint == NULL) return NULL;
  float level;
  endpoint->GetMasterVolumeLevelScalar(&level);
  return level * 100;
}

bool VolumeControl::get_master_mute() {
  IAudioEndpointVolume* endpoint = GetMaster();
  if (endpoint == NULL) return NULL;
  BOOL mute;
  endpoint->GetMute(&mute);
  return mute;
}

float VolumeControl::get_app_volume(int pid) {
  ISimpleAudioVolume* volume = GetSession(pid);
  if (volume == NULL) return NULL;
  float level = NULL;
  volume->GetMasterVolume(&level);
  SAFE_RELEASE(volume);
  return level * 100;
}

bool VolumeControl::get_app_mute(int pid) {
  ISimpleAudioVolume* volume = GetSession(pid);
  if (volume == NULL) return NULL;
  BOOL mute = NULL;
  volume->GetMute(&mute);
  SAFE_RELEASE(volume);
  return mute;
}

void VolumeControl::set_master_volume(float level) {
  IAudioEndpointVolume* endpoint = GetMaster();
  if (endpoint == NULL) return;
  endpoint->SetMasterVolumeLevelScalar(level / 100, NULL);
  SAFE_RELEASE(endpoint);
}

void VolumeControl::set_master_mute(bool mute) {
  IAudioEndpointVolume* endpoint = GetMaster();
  if (endpoint == NULL) return;
  endpoint->SetMute(mute, NULL);
  SAFE_RELEASE(endpoint);
}

void VolumeControl::set_app_volume(int pid, float level) {
  ISimpleAudioVolume* volume = GetSession(pid);
  if (volume == NULL) return;
  volume->SetMasterVolume(level / 100, NULL);
  SAFE_RELEASE(volume);
}

void VolumeControl::set_app_mute(int pid, bool mute) {
  ISimpleAudioVolume* volume = GetSession(pid);
  if (volume == NULL) return;
  volume->SetMute(mute, NULL);
  SAFE_RELEASE(volume);
}

std::vector<int> VolumeControl::get_all_pid() {
  std::vector<int> ret;

  auto cb = [&ret](IAudioSessionControl*, IAudioSessionControl2*, DWORD pid) {
    ret.push_back(pid);
    return false;
  };

  session_enumerate(cb);

  return ret;
}

std::vector<VolumeControl::AudioSessionInfo> VolumeControl::get_all_sessions_info() {

  std::vector<AudioSessionInfo> ret;

  auto cb = [&ret](IAudioSessionControl* ctrl, IAudioSessionControl2* ctrl2, DWORD pid) {

    AudioSessionInfo info{};
    info.pid_ = pid;
    ISimpleAudioVolume* volume;
    if (wrapped_call(ctrl2->QueryInterface(__uuidof(ISimpleAudioVolume), (void**)&volume))) {
      float vol = 0;
      volume->GetMasterVolume(&vol);
      info.volume_ = vol * 100.0f;
      BOOL b;
      volume->GetMute(&b);
      info.muted_ = b;
      SAFE_RELEASE(volume);
    }

    LPWSTR str;
    ctrl->GetDisplayName(&str);
    info.display_name_ = std::wstring(str);
    CoTaskMemFree(str);

    ctrl->GetIconPath(&str);
    info.icon_path_ = std::wstring(str);
    CoTaskMemFree(str);
    ret.push_back(info);
    return false;
  };

  session_enumerate(cb);
  return ret;
}

VolumeControl::AudioSessionInfo VolumeControl::get_master_info() {

  AudioSessionInfo info;
  info.pid_ = -1;
  info.display_name_ = L"Master";
  info.volume_ = get_master_volume();
  info.muted_ = get_master_mute();

  return info;

}
