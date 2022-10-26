#pragma once
#include <vector>
#include <string>

struct CommPortDesc {
    std::string dev_descr_;
    std::string bus_reported_dev_descr_;
    std::string friendly_name_;
    std::string port_str_;
};

std::vector<CommPortDesc> get_com_ports();
