#include "VolumeAPI.h"
#include <functional>
#include <windows.h>
#include <mmdeviceapi.h>
#include <audiopolicy.h>
#include <endpointvolume.h>
#include <Psapi.h>
#include <filesystem>
#include <Windows.h>
#include <shellapi.h>

namespace fs = std::filesystem;

// Based on: https://github.com/chrispader/VolumeControl

#define SAFE_RELEASE(x)                                                                                                \
  if (x) {                                                                                                             \
    x->Release();                                                                                                      \
    x = NULL;                                                                                                          \
  }


std::wostream& operator<<(std::wostream& out, const VolumeControl::AudioSessionInfo& audio) {
  out << audio.filename_ << std::wstring(L"\n\tpath: ") << audio.path_ << std::wstring(L"\n\tvolume: ")
      << std::to_wstring(audio.volume_) << std::wstring(L"\n\tmuted: ") << std::to_wstring(audio.muted_)
      << std::wstring(L"\n\ticon: ") << audio.icon_data_.size() << std::wstring(L"\n\tPID: ")
      << std::to_wstring(audio.pid_);

  return out;
}


struct ICONDIRENTRY {
  UCHAR nWidth;
  UCHAR nHeight;
  UCHAR nNumColorsInPalette;  // 0 if no palette
  UCHAR nReserved;            // should be 0
  WORD nNumColorPlanes;       // 0 or 1
  WORD nBitsPerPixel;
  ULONG nDataLength;  // length in bytes
  ULONG nOffset;      // offset of BMP or PNG data from beginning of file
};


bool GetIconData(HICON hIcon, int nColorBits, std::vector<char>& buff) {
  if (offsetof(ICONDIRENTRY, nOffset) != 12) {
    return false;
  }

  HDC dc = CreateCompatibleDC(NULL);


  // Write header:
  char icoHeader[6] = { 0, 0, 1, 0, 1, 0 };  // ICO file with 1 image
  buff.insert(buff.end(), reinterpret_cast<const char*>(icoHeader),
              reinterpret_cast<const char*>(icoHeader) + sizeof(icoHeader));

  // Get information about icon:
  ICONINFO iconInfo;
  GetIconInfo(hIcon, &iconInfo);
  BITMAPINFO bmInfo = { 0 };
  bmInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bmInfo.bmiHeader.biBitCount = 0;  // don't get the color table
  if (!GetDIBits(dc, iconInfo.hbmColor, 0, 0, NULL, &bmInfo, DIB_RGB_COLORS)) {
    return false;
  }

  // Allocate size of bitmap info header plus space for color table:
  int nBmInfoSize = sizeof(BITMAPINFOHEADER);
  if (nColorBits < 24) {
    nBmInfoSize += sizeof(RGBQUAD) * (int)(1 << nColorBits);
  }

  std::vector<UCHAR> bitmapInfo;
  bitmapInfo.resize(nBmInfoSize);
  BITMAPINFO* pBmInfo = (BITMAPINFO*)bitmapInfo.data();
  memcpy(pBmInfo, &bmInfo, sizeof(BITMAPINFOHEADER));

  // Get bitmap data:
  if (!bmInfo.bmiHeader.biSizeImage) return false;
  std::vector<UCHAR> bits;
  bits.resize(bmInfo.bmiHeader.biSizeImage);
  pBmInfo->bmiHeader.biBitCount = nColorBits;
  pBmInfo->bmiHeader.biCompression = BI_RGB;
  if (!GetDIBits(dc, iconInfo.hbmColor, 0, bmInfo.bmiHeader.biHeight, bits.data(), pBmInfo, DIB_RGB_COLORS)) {
    return false;
  }

  // Get mask data:
  BITMAPINFO maskInfo = { 0 };
  maskInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  maskInfo.bmiHeader.biBitCount = 0;  // don't get the color table
  if (!GetDIBits(dc, iconInfo.hbmMask, 0, 0, NULL, &maskInfo, DIB_RGB_COLORS) || maskInfo.bmiHeader.biBitCount != 1)
    return false;

  std::vector<UCHAR> maskBits;
  maskBits.resize(maskInfo.bmiHeader.biSizeImage);
  std::vector<UCHAR> maskInfoBytes;
  maskInfoBytes.resize(sizeof(BITMAPINFO) + 2 * sizeof(RGBQUAD));
  BITMAPINFO* pMaskInfo = (BITMAPINFO*)maskInfoBytes.data();
  memcpy(pMaskInfo, &maskInfo, sizeof(maskInfo));
  if (!GetDIBits(dc, iconInfo.hbmMask, 0, maskInfo.bmiHeader.biHeight, maskBits.data(), pMaskInfo, DIB_RGB_COLORS)) {
    return false;
  }

  // Write directory entry:
  ICONDIRENTRY dir;
  dir.nWidth = (UCHAR)pBmInfo->bmiHeader.biWidth;
  dir.nHeight = (UCHAR)pBmInfo->bmiHeader.biHeight;
  dir.nNumColorsInPalette = (nColorBits == 4 ? 16 : 0);
  dir.nReserved = 0;
  dir.nNumColorPlanes = 0;
  dir.nBitsPerPixel = pBmInfo->bmiHeader.biBitCount;
  dir.nDataLength = pBmInfo->bmiHeader.biSizeImage + pMaskInfo->bmiHeader.biSizeImage + nBmInfoSize;
  dir.nOffset = sizeof(dir) + sizeof(icoHeader);

  buff.insert(buff.end(), reinterpret_cast<const char*>(&dir), reinterpret_cast<const char*>(&dir) + sizeof(dir));

  // Write DIB header (including color table):
  int nBitsSize = pBmInfo->bmiHeader.biSizeImage;
  pBmInfo->bmiHeader.biHeight *= 2;  // because the header is for both image and mask
  pBmInfo->bmiHeader.biCompression = 0;
  pBmInfo->bmiHeader.biSizeImage += pMaskInfo->bmiHeader.biSizeImage;  // because the header is for both image and mask

  buff.insert(buff.end(), reinterpret_cast<const char*>(&pBmInfo->bmiHeader),
              reinterpret_cast<const char*>(&pBmInfo->bmiHeader) + nBmInfoSize);

  // Write image data:
  buff.insert(buff.end(), reinterpret_cast<const char*>(bits.data()),
              reinterpret_cast<const char*>(bits.data()) + nBitsSize);

  // Write mask data:
  buff.insert(buff.end(), reinterpret_cast<const char*>(maskBits.data()),
              reinterpret_cast<const char*>(maskBits.data()) + pMaskInfo->bmiHeader.biSizeImage);


  DeleteObject(iconInfo.hbmColor);
  DeleteObject(iconInfo.hbmMask);

  DeleteDC(dc);

  return true;
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

    info.path_ = ProcessAPI::get_path_from_pid(pid);

    info.filename_ = fs::path(info.path_).filename().stem().wstring();

    info.icon_data_ = ProcessAPI::get_icon_from_pid(pid);
    ret.push_back(info);
    return false;
  };

  session_enumerate(cb);
  return ret;
}

VolumeControl::AudioSessionInfo VolumeControl::get_master_info() {
  AudioSessionInfo info;
  info.filename_ = L"Master";
  info.pid_ = -1;
  ;
  info.volume_ = get_master_volume();
  info.muted_ = get_master_mute();

  return info;
}


std::wstring ProcessAPI::get_path_from_pid(int pid) {
  HANDLE process_handle = NULL;
  TCHAR path[MAX_PATH];
  // PROCESS_QUERY_INFORMATION | PROCESS_VM_READ
  process_handle = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);

  if (process_handle == NULL) {
    return {};
  }

  if (GetModuleFileNameEx(process_handle, NULL, path, MAX_PATH)) {
    CloseHandle(process_handle);
    return std::wstring(path);
  }
  CloseHandle(process_handle);
  return {};
}


std::vector<char> ProcessAPI::get_icon_from_pid(int pid) {
  std::wstring path = get_path_from_pid(pid);

  UINT num_icons = ExtractIconEx(path.c_str(), -1, NULL, NULL, 0);

  std::vector<char> out;
  if (num_icons) {
    HICON ilarge{};
    ExtractIconEx(path.c_str(), 0, &ilarge, NULL, 1);
    GetIconData(ilarge, 32, out);
    DestroyIcon(ilarge);
  }

  return out;
}
