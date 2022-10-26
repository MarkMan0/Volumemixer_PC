#include "VolumeAPI/VolumeAPI.h"
#include "SerialPortWrapper/SerialPortWrapper.h"
#include <thread>
#include <atomic>
#include <chrono>
#include "communication.h"
#include <memory>


enum state_t : uint8_t {
  PORT_CLOSE = 0,
  PORT_SEARCHING,
  PORT_OPEN,
  NUM_STATES,
};

enum event_t : uint8_t {
  EVENT_SUCCESS = 0,
  EVENT_FAILURE,
  NUM_EVENTS,
};


event_t port_close_handler();
event_t port_searching_handler();
event_t port_open_handler();

static constexpr size_t num_states = static_cast<size_t>(state_t::NUM_STATES);
static constexpr size_t num_events = static_cast<size_t>(event_t::NUM_EVENTS);

using fcn_t = event_t (*)(void);


std::unique_ptr<SerialPortWrapper> gPort{ nullptr };

int main() {
  static fcn_t state_table[] = { port_close_handler, port_searching_handler, port_open_handler };
  static state_t transition_table[num_states][num_events] = { { PORT_SEARCHING, PORT_SEARCHING },
                                                              { PORT_OPEN, PORT_SEARCHING },
                                                              { PORT_OPEN, PORT_CLOSE } };

  if (not VolumeControl::init()) {
    return -1;
  }

  state_t curr_state = state_t::PORT_SEARCHING;


  DEBUG_PRINT("Entering main loop\n");
  while (1) {
    DEBUG_PRINT("While loop...\n");

    fcn_t curr_handler = state_table[curr_state];
    if (curr_handler) {
      event_t ev = curr_handler();
      curr_state = transition_table[curr_state][ev];
    } else {
      break;
    }
  }
}


event_t port_close_handler() {
  gPort->close();
  return event_t::EVENT_SUCCESS;
}

event_t port_searching_handler() {
  // TODO
  gPort = std::make_unique<SerialPortWrapper>(8, 115200);
  gPort->open();
  if ((*gPort)()) {
    return event_t::EVENT_SUCCESS;
  } else {
    // if can't open, sleep
    std::this_thread::sleep_for(std::chrono::seconds(10));
    return event_t::EVENT_FAILURE;
  }
}

event_t port_open_handler() {
  if (serial_comm(*gPort)) {
    return event_t::EVENT_SUCCESS;
  } else {
    return event_t::EVENT_FAILURE;
  }
}