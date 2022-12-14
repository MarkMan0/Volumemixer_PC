#include "process_api.h"
#include <Windows.h>
#include <shellapi.h>
#include <Psapi.h>
#include <filesystem>
#include <memory>
#include <locale>
#include <codecvt>
#include <fstream>
#include "special_icons.h"

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

/// @brief Load the icon data from the handle to a vector
/// @param hIcon icon handle
/// @param nColorBits color depth
/// @param buff output buffer
/// @return true on success
static bool GetIconData(HICON hIcon, int nColorBits, std::vector<uint8_t>& buff) {
  HDC dc = 0;


  int nBmInfoSize = sizeof(BITMAPINFOHEADER);
  std::vector<UCHAR> bitmapInfo;
  BITMAPINFO* pBmInfo = 0;
  std::vector<UCHAR> bits;
  BITMAPINFO maskInfo = { 0 };
  std::vector<UCHAR> maskBits;
  std::vector<UCHAR> maskInfoBytes;
  BITMAPINFO* pMaskInfo = 0;
  ICONDIRENTRY dir{};
  int nBitsSize = 0;


  if (offsetof(ICONDIRENTRY, nOffset) != 12) {
    return false;
  }

  dc = CreateCompatibleDC(NULL);


  // Write header:
  uint8_t icoHeader[6] = { 0, 0, 1, 0, 1, 0 };  // ICO file with 1 image
  buff.insert(buff.end(), icoHeader, icoHeader + sizeof(icoHeader));

  // Get information about icon:
  ICONINFO iconInfo;
  GetIconInfo(hIcon, &iconInfo);
  BITMAPINFO bmInfo = { 0 };
  bmInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bmInfo.bmiHeader.biBitCount = 0;  // don't get the color table
  if (!GetDIBits(dc, iconInfo.hbmColor, 0, 0, NULL, &bmInfo, DIB_RGB_COLORS)) {
    goto error;
  }

  // Allocate size of bitmap info header plus space for color table:
  if (nColorBits < 24) {
    nBmInfoSize += sizeof(RGBQUAD) * (int)(1 << nColorBits);
  }


  bitmapInfo.resize(nBmInfoSize);
  pBmInfo = (BITMAPINFO*)bitmapInfo.data();
  memcpy(pBmInfo, &bmInfo, sizeof(BITMAPINFOHEADER));

  // Get bitmap data:
  if (!bmInfo.bmiHeader.biSizeImage) {
    goto error;
  }

  bits.resize(bmInfo.bmiHeader.biSizeImage);
  pBmInfo->bmiHeader.biBitCount = nColorBits;
  pBmInfo->bmiHeader.biCompression = BI_RGB;
  if (!GetDIBits(dc, iconInfo.hbmColor, 0, bmInfo.bmiHeader.biHeight, bits.data(), pBmInfo, DIB_RGB_COLORS)) {
    goto error;
  }

  // Get mask data:
  maskInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  maskInfo.bmiHeader.biBitCount = 0;  // don't get the color table
  if (!GetDIBits(dc, iconInfo.hbmMask, 0, 0, NULL, &maskInfo, DIB_RGB_COLORS) || maskInfo.bmiHeader.biBitCount != 1) {
    goto error;
  }


  maskBits.resize(maskInfo.bmiHeader.biSizeImage);
  maskInfoBytes.resize(sizeof(BITMAPINFO) + 2 * sizeof(RGBQUAD));
  pMaskInfo = (BITMAPINFO*)maskInfoBytes.data();
  memcpy(pMaskInfo, &maskInfo, sizeof(maskInfo));
  if (!GetDIBits(dc, iconInfo.hbmMask, 0, maskInfo.bmiHeader.biHeight, maskBits.data(), pMaskInfo, DIB_RGB_COLORS)) {
    goto error;
  }

  // Write directory entry:

  dir.nWidth = (UCHAR)pBmInfo->bmiHeader.biWidth;
  dir.nHeight = (UCHAR)pBmInfo->bmiHeader.biHeight;
  dir.nNumColorsInPalette = (nColorBits == 4 ? 16 : 0);
  dir.nReserved = 0;
  dir.nNumColorPlanes = 0;
  dir.nBitsPerPixel = pBmInfo->bmiHeader.biBitCount;
  dir.nDataLength = pBmInfo->bmiHeader.biSizeImage + pMaskInfo->bmiHeader.biSizeImage + nBmInfoSize;
  dir.nOffset = sizeof(dir) + sizeof(icoHeader);

  buff.insert(buff.end(), reinterpret_cast<const uint8_t*>(&dir), reinterpret_cast<const uint8_t*>(&dir) + sizeof(dir));

  // Write DIB header (including color table):
  nBitsSize = pBmInfo->bmiHeader.biSizeImage;
  pBmInfo->bmiHeader.biHeight *= 2;  // because the header is for both image and mask
  pBmInfo->bmiHeader.biCompression = 0;
  pBmInfo->bmiHeader.biSizeImage += pMaskInfo->bmiHeader.biSizeImage;  // because the header is for both image and mask

  buff.insert(buff.end(), reinterpret_cast<const uint8_t*>(&pBmInfo->bmiHeader),
              reinterpret_cast<const uint8_t*>(&pBmInfo->bmiHeader) + nBmInfoSize);

  // Write image data:
  buff.insert(buff.end(), reinterpret_cast<const uint8_t*>(bits.data()),
              reinterpret_cast<const uint8_t*>(bits.data()) + nBitsSize);

  // Write mask data:
  buff.insert(buff.end(), reinterpret_cast<const uint8_t*>(maskBits.data()),
              reinterpret_cast<const uint8_t*>(maskBits.data()) + pMaskInfo->bmiHeader.biSizeImage);


  DeleteObject(iconInfo.hbmColor);
  DeleteObject(iconInfo.hbmMask);

  DeleteDC(dc);
  return true;
error:
  DeleteDC(dc);
  return false;
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


static std::vector<uint8_t> get_process_icon_from_pid(int pid) {
  std::wstring path = ProcessAPI::get_path_from_pid(pid);

  UINT num_icons = ExtractIconEx(path.c_str(), -1, NULL, NULL, 0);

  std::vector<uint8_t> out;
  if (num_icons) {
    HICON ilarge{};
    ExtractIconEx(path.c_str(), 0, &ilarge, NULL, 1);
    GetIconData(ilarge, 32, out);
    DestroyIcon(ilarge);
  }

  return out;
}

static std::vector<uint8_t> get_process_png_from_pid(int pid) {
  // get .ico data
  const auto ico_data = get_process_icon_from_pid(pid);

  // get temp directory path
  namespace fs = std::filesystem;
  const auto temp_dir = fs::temp_directory_path();

  // create unique files with needed extensions
  const auto ico_path = temp_dir / L"VOL001.ico";
  const auto png_path = temp_dir / L"VOL001.png";


  // write .ico file to temp
  std::ofstream ico_file(ico_path, std::ios::out | std::ios::binary);
  for (const auto val : ico_data) {
    ico_file << val;
  }
  ico_file.close();  // make sure to close before going further

  // construct command to convert .ico to .png
  std::wstring command;
  command += L"magick ";
  command += ico_path.wstring();
  command += L" ";
  command += png_path.wstring();
  std::string cmd = std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t>().to_bytes(command);

  // call magick
  std::system(cmd.c_str());

  // read .png file
  std::ifstream png_file(png_path, std::ios::in | std::ios::binary);

  std::vector<uint8_t> png_data((std::istreambuf_iterator<char>(png_file)), std::istreambuf_iterator<char>());

  png_file.close();
  fs::remove(ico_path);
  fs::remove(png_path);

  return png_data;
}


std::vector<uint8_t> ProcessAPI::get_png_from_pid(int pid) {
  if (pid == -1) {
    // master icon
    return std::vector<uint8_t>(icon_master.begin(), icon_master.end());
  }
  if (pid == 0) {
    // system icon
    return std::vector<uint8_t>(icon_system.begin(), icon_system.end());
  }

  return get_process_png_from_pid(pid);
}