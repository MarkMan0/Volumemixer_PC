#pragma once
#include <vector>
#include <string>

struct CommPortDesc {
  std::wstring dev_descr_;
  std::wstring bus_reported_dev_descr_;
  std::wstring friendly_name_;
  std::wstring port_str_;
};

std::vector<CommPortDesc> get_com_ports();
