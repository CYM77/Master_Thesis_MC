#include "addr_mapper/addr_mapper.h"
#include "base/request.h"
#include "dram/dram.h"
#include "dram_controller/controller.h"
#include "memory_system/memory_system.h"
#include "translation/translation.h"
#include <deque>

namespace Ramulator {

class GenericDRAMSystem final : public IMemorySystem, public Implementation {
  RAMULATOR_REGISTER_IMPLEMENTATION(IMemorySystem, GenericDRAMSystem,
                                    "GenericDRAM",
                                    "A generic DRAM-based memory system.");
  // I suppose should reorder and ensure the data in-order property is
  // maintained We use callbank function to inform the frontend the request has
  // ended thus returning the control

protected:
  Clk_t m_clk = 0;
  IDRAM *m_dram;
  IAddrMapper *m_addr_mapper;
  std::vector<IDRAMController *> m_controllers;
  std::deque<Request> m_read_in_order_q;
  std::deque<Request> m_receive_merge_q;
  

public:
  int s_num_read_requests = 0;
  int s_num_write_requests = 0;
  int s_num_other_requests = 0;
  float s_average_bandwidth = 0;
  float s_peak_bandwidth = 0;
  float s_worst_bandwidth = 0;

public:
  void init() override {
    // Create device (a top-level node wrapping all channel nodes)
    m_dram = create_child_ifce<IDRAM>();
    m_addr_mapper = create_child_ifce<IAddrMapper>();

    int num_channels = m_dram->get_level_size("channel");

    // Create memory controllers
    for (int i = 0; i < num_channels; i++) {
      IDRAMController *controller = create_child_ifce<IDRAMController>();
      controller->m_impl->set_id(fmt::format("Channel {}", i));
      controller->m_channel_id = i;
      m_controllers.push_back(controller);
    }

    m_logger = Logging::create_logger("GenericDRAMSystemLog");

    m_clock_ratio = param<uint>("clock_ratio").required();

    register_stat(m_clk).name("memory_system_cycles");
    register_stat(s_num_read_requests).name("total_num_read_requests");
    register_stat(s_num_write_requests).name("total_num_write_requests");
    register_stat(s_num_other_requests).name("total_num_other_requests");

    register_stat(s_average_bandwidth).name("average_bandwidth");
    register_stat(s_peak_bandwidth).name("peak_bandwidth");
    register_stat(s_worst_bandwidth).name("worst_bandwidth");
  };

  void setup(IFrontEnd *frontend, IMemorySystem *memory_system) override {}

  bool send(Request req) override {
    m_addr_mapper->apply(req);
    int channel_id = req.addr_vec[0];

    // check rd write queue, 

    // To debug: For test Case 1 @ core 36 to 1 channel, the core_id 36 is never served, never a success
    // Since core_id 36 is never served, the simulation would never completes.
    // 1. Why other cores still keep simulating even if the core_id 36 is never served?
    // 2. Why core_id 36 is never served?
    bool is_success = m_controllers[channel_id]->send(req); // Sends the request to the controller


    if (is_success) {
      switch (req.type_id) {
      case Request::Type::Read: {
        s_num_read_requests++;
        // m_logger->debug("Enqueueing read request at Clk={}, Addr={}, Type={}",
        //                 m_clk, req.addr, req.type_id);
        m_read_in_order_q.push_back(req);
        break;
      }
      case Request::Type::Write: {
        s_num_write_requests++;
        // m_logger->debug("Enqueueing write request at Clk={}, Addr={}, Type={}",
        //                 m_clk, req.addr, req.type_id);
        break;
      }
      default: {
        s_num_other_requests++;
        break;
      }
      }
    }

    return is_success;
  };

  bool ordered_receive(Request req) override {
    m_receive_merge_q.push_back(req);
    return false;
  }

  void tick() override {
    m_clk++;
    m_dram->tick();


    for (auto controller : m_controllers) {
      controller->tick(); // Tick the controller
    }
    in_order_callback();
  };

  float get_tCK() override { return m_dram->m_timing_vals("tCK_ps") / 1000.0f; }

private:
  void in_order_callback() {
    // First check if the receive merge queue is empty
    if (m_receive_merge_q.empty()) {
      return;
    }

    // reorder the merge queue with the arrival time
    std::sort(m_receive_merge_q.begin(), m_receive_merge_q.end(),
              [](const Request &a, const Request &b) {
                return a.arrive < b.arrive;
              });

    // Traverse the whole m_receive_merge_q
    for (auto it = m_receive_merge_q.begin(); it != m_receive_merge_q.end();) {
      // check if the request is the same as the first request in the in order
      // queue
      if (m_read_in_order_q.front().arrive == it->arrive) {
        // If the request is the same as the first request in the in order queue
        // then we can send the request to the frontend
        // m_logger->debug(
        //     "Sending request to frontend at Clk={}, Addr={}, Type={}", m_clk,
        //     it->addr, it->type_id);

        Request req_to_callback = m_read_in_order_q.front();

        it = m_receive_merge_q.erase(it);
        m_read_in_order_q.pop_front();

        // TO-DO Callback to the frontend if it has a callback to do
        if (req_to_callback.callback != nullptr) {
          // std:: cout << "Callback to the frontend" << std::endl;
          req_to_callback.callback(req_to_callback);
        }
      } else {
        // If the request is not the same as the first request in the in order
        // queue then we can break the loop
        break;
      }
    }
  }

  // const SpecDef& get_supported_requests() override {
  //   return m_dram->m_requests;
  // };
};

} // namespace Ramulator
