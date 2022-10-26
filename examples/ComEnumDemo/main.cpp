#include <iostream>
#include "ComEnum/ComEnum.h"

int main() {
  std::cout << "Hello World\n";
  auto ports = get_com_ports();

  for (const auto& port : ports) {
    std::wcout << port.port_str_ << "\t->\t" << port.bus_reported_dev_descr_ << '\n';
  }
}
