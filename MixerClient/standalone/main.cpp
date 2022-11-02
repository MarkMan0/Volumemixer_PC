#include "client.h"

int main() {
  if (0 != client_init()) {
    return -1;
  }
  while (0 == client_main()) {
  }
}
