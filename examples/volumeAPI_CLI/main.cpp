#include <iostream>
#include "VolumeAPI/VolumeAPI.h"
#include <fstream>
#include <string>
#include <optional>



static void print_help();

static void flush_cin() {
  std::cin.clear();
  std::cin.ignore(10000, '\n');
}

int main() {
  if (not VolumeControl::init()) {
    return 1;
  }

  bool running = true;
  std::optional<int> selected;

  while (running) {
    auto info = VolumeControl::get_all_sessions_info();

    for (const auto& i : info) {
      std::wcout << i << '\n';
    }

    char c = std::getchar();


    switch (c) {
      case 'q':
        running = false;
        break;

      case 'h':
        print_help();
        break;

      case 's': {
        int pid;
        std::cin >> pid;
        selected = std::nullopt;
        for (const auto& i : info) {
          if (i.pid_ == pid) {
            selected = pid;
            break;
          }
        }
      } break;

      case 'l':
        if (selected) {
          for (const auto& i : info) {
            if (i.pid_ == *selected) {
              std::cout << "IMG len: " << i.get_icon_data().size() << '\n';
              break;
            }
          }
        }

      case '+':
        if (selected) {
          int vol = VolumeControl::get_volume(*selected);
          VolumeControl::set_volume(*selected, vol + 5);
        }
        break;

      case '-':
        if (selected) {
          int vol = VolumeControl::get_volume(*selected);
          VolumeControl::set_volume(*selected, vol - 5);
        }
        break;
    }
    flush_cin();
  }


  return 0;
}



void print_help() {
  std::cout << "q: Quit\ns<pid>: select audio\n+: increase volume\n-: decrease volume\nh: print help\n";
}