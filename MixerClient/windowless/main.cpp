#include <Windows.h>
#include "client.h"
#include <cstdio>


int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
  if (0 != client_init()) {
    return -1;
  }
  while (0 == client_main()) {
  }
}
