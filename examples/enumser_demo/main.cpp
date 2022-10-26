#include <iostream>
#include "enumser/enumser.h"
#include <vector>


int main() {
  HRESULT hr{ CoInitialize(nullptr) };

  CEnumerateSerial::CPortAndNamesArray portAndNames;

  if (CEnumerateSerial::UsingSetupAPI2(portAndNames)) {
    for (const auto& port : portAndNames) {
      std::wcout << "COM" << port.first << " <" << port.second.c_str() << '>' << '\n';
    }
  } else {
    std::wcout << "CEnumerateSerial::UsingSetupAPI2 failed, Error:" << GetLastError() << '\n';
  }
}