#include "process_api.h"
#include <Windows.h>
#include <shellapi.h>
#include <Psapi.h>


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
static bool GetIconData(HICON hIcon, int nColorBits, std::vector<char>& buff) {
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
