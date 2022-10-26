#include <iostream>
#include "enumser/enumser.h"
#include <vector>


int main() {
  HRESULT hr{ CoInitialize(nullptr) };
  hr = CoInitializeSecurity(nullptr, -1, nullptr, nullptr, RPC_C_AUTHN_LEVEL_DEFAULT, RPC_C_IMP_LEVEL_IMPERSONATE,
                            nullptr, EOAC_NONE, nullptr);
  CEnumerateSerial::CPortsArray ports;
  CEnumerateSerial::CPortAndNamesArray portAndNames;
  CEnumerateSerial::CNamesArray names;
  ULONGLONG nStartTick{ 0 };


  _tprintf(_T("Device Manager (SetupAPI - Ports Device information set) reports\n"));
  nStartTick = GetTickCount64();
  if (CEnumerateSerial::UsingSetupAPI2(portAndNames)) {
    for (const auto& port : portAndNames)
#pragma warning(suppress : 26489)
      _tprintf(_T("COM%u <%s>\n"), port.first, port.second.c_str());
  } else
    _tprintf(_T("CEnumerateSerial::UsingSetupAPI2 failed, Error:%u\n"),
             GetLastError());  // NOLINT(clang-diagnostic-format)
  _tprintf(_T(" Time taken: %I64u ms\n"), GetTickCount64() - nStartTick);
}