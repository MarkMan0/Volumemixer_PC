
#include "ServiceHelper/ServiceMain.h"
#include <atomic>
#include "client.h"

class MixerService {
public:
  int run() {
    if (0 != client_init()) {
      state_ = EXIT_FAILURE;
      return state();
    }

    while (running_ && state_ == EXIT_SUCCESS) {
      if (0 != client_main()) {
        running_ = false;
        state_ = EXIT_FAILURE;
      }
    }

    client_deinit();
    return state();
  }

  void stop() {
    running_ = false;
  }

  //@return 0 for A_OK and anything else for failures.
  int state() const {
    return state_;
  }

private:
  std::atomic_bool running_ = true;
  int state_ = EXIT_SUCCESS;
};


#define SERVICENAME TEXT("VolumeMixerGadgetService")
#define DISPLAYNAME TEXT("External Volume Mixer Windows Client")  // displayed in Windows Services
#define DESCRIPTION TEXT("Communicates with the external volume mixer through a serial port")

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

  ServiceMain<MixerService>(name.get(), display_name.get(), description.get(), argv);
}