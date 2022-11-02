// service.cpp
//
// Author: Mikko Saarinki
// Copyright (c) 2016 Mikko Saarinki. All rights reserved.
//
#include "ServiceHelper/ServiceMain.h"
#include <memory>
#include "ExampleApp.h"

#define SERVICENAME TEXT("ExampleApp")
#define DISPLAYNAME TEXT("Windows Service Example")  // displayed in Windows Services
#define DESCRIPTION TEXT("Does nothing but sleeps and waits for service stop signal.")

static std::unique_ptr<TCHAR[]> to_mutable(LPCTSTR str) {
  auto len = _tcslen(str);
  std::unique_ptr ptr = std::make_unique<TCHAR[]>(len + 1);
  _tcscpy(ptr.get(), str);
  return std::move(ptr);
}

int __cdecl _tmain(int argc, TCHAR* argv[]) {
  std::unique_ptr name = to_mutable(SERVICENAME);
  std::unique_ptr display_name = to_mutable(DISPLAYNAME);
  std::unique_ptr description = to_mutable(DESCRIPTION);

  ServiceMain<ExampleApp>(name.get(), display_name.get(), description.get(), argv);
}
