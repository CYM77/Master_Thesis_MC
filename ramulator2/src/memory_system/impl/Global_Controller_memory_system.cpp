//Global_Controller_memory_system RAW w/o write buffer 
#include "addr_mapper/addr_mapper.h"
#include "base/request.h"
#include "dram/dram.h"
#include "dram_controller/controller.h"
#include "memory_system/memory_system.h"
#include "translation/translation.h"
#include <deque>
#include "dram_controller/scheduler.h"
#include <fstream>

namespace Ramulator {

class Global_Controller_DRAMSystem final : public IMemorySystem, public Implementation {
  RAMULATOR_REGISTER_IMPLEMENTATION(IMemorySystem, Global_Controller_DRAMSystem,
                                    "Global_Controller",
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

  ReqBuffer read_request_buffer;
  ReqBuffer write_request_buffer;
  int write_flush_threshold; // Set the threshold for write flush
  bool write_flush_flag; // Set the flag for write flush
  
public:
  int s_num_read_requests = 0;
  int s_num_write_requests = 0;
  int s_num_other_requests = 0;
  float s_average_bandwidth = 0;
  float s_peak_bandwidth = 0;
  float s_worst_bandwidth = 0;
  IScheduler*   m_scheduler = nullptr;
 

public:
  void init() override {
    //set R/W queue size
    read_request_buffer.set_queue_size(8 - 1);
    write_request_buffer.set_queue_size(8 - 1);
    write_flush_flag = false;
    write_flush_threshold = 4;

    // m_scheduler = create_child_ifce<IScheduler>();////////


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

  

  //20250506
  //define issue function
  //check R/W queue when tick
  //define a new flag for write flush
  //write flush condition
  //1. if the write queue length is larger than the threshold(watermark)
  //2. if the read request has RAW dependency with the write request
  // void issue(){
  //   ReqBuffer::iterator req_candidate;
  //   if(read_request_buffer.size() != 0 && write_flush_flag == false){
  //     req_candidate = m_scheduler->get_best_request(read_request_buffer);
  //     if(RAW_check(*req_candidate, write_request_buffer)){
  //       //if the read request has RAW dependency with the write request
  //       //then we need to flush the write request
  //       write_flush_flag = true;
  //     }
  //     else{
  //       // if(write_request_buffer.size() != 0){
  //       //   req_candidate = m_scheduler->get_best_request(write_request_buffer);
  //       // }  
  //     }
  //   }
  //   else if(write_request_buffer.size() != 0){
  //     req_candidate = m_scheduler->get_best_request(write_request_buffer);
  //   }
  //   else{
  //     return;
  //   }
  //   //issue the request
  //   int channel_id = req_candidate->addr_vec[0];

  //   bool is_success = m_controllers[channel_id]->send(*req_candidate);

  //   if (is_success) {
  //     switch (req_candidate->type_id) {
  //     case Request::Type::Read: {
  //       std::cout << "Read request"<< req_candidate->addr_vec[0] << "is issued to channel " << channel_id << std::endl;
  //       s_num_read_requests++;
  //       // m_logger->debug("Enqueueing read request at Clk={}, Addr={}, Type={}",
  //       //                 m_clk, req.addr, req.type_id);
  //       m_read_in_order_q.push_back(*req_candidate);
  //       read_request_buffer.remove(req_candidate);
  //       break;
  //     }
  //     case Request::Type::Write: {
  //       std::cout << "Write request"<< req_candidate->addr_vec[0] << "is issued to channel " << channel_id << std::endl;
  //       s_num_write_requests++;
  //       // m_logger->debug("Enqueueing write request at Clk={}, Addr={}, Type={}",
  //       //                 m_clk, req.addr, req.type_id);
  //       write_request_buffer.remove(req_candidate);
  //       break;
  //     }
  //     default: {
  //       s_num_other_requests++;
  //       break;
  //     }
  //     }
  //   }


  //   //determine read or write request buffer be issued
  //   if (write_flush_flag) {
  //     if (write_request_buffer.size() == 0) {
  //       write_flush_flag = false;
  //     }
  //   } else {
  //     if (write_request_buffer.size() >= write_flush_threshold) {
  //       write_flush_flag = true;
  //     }
  //   }

  //   return;
  // }

  void print_q_element() {
    // if (m_clk <= 40) {
        std::ofstream log_file("/home/cym/ym_training/Master_Thesis_MC/ramulator2/src/memory_system/impl/log.txt", std::ios::app); // 用 append 模式寫入
        if (!log_file.is_open()) {
            std::cerr << "Failed to open log file." << std::endl;
            return;
        }
        log_file << "====================" << std::endl;
        log_file << "m_clk: " << m_clk << std::endl;
        log_file << "write flush flag: " << write_flush_flag << std::endl;
        //print size of element in read request buffer
        // std::cout << "read request buffer size: " << read_request_buffer.size() << std::endl;
        // std::cout << "write request buffer size: " << write_request_buffer.size() << std::endl;
        // std::cout << "read in-order request buffer size: " << m_read_in_order_q.size() << std::endl;
        // std::cout << "receive merge queue size: " << m_receive_merge_q.size() << std::endl;

       log_file << "read request buffer size: " << read_request_buffer.size() << std::endl;
       log_file << "write request buffer size: " << write_request_buffer.size() << std::endl;
       log_file << "read in-order request buffer size: " << m_read_in_order_q.size() << std::endl;
       log_file << "receive merge queue size: " << m_receive_merge_q.size() << std::endl;
       log_file << "\n" << std::endl;
       //print element in read request buffer
        for (auto it = read_request_buffer.buffer.begin(); it != read_request_buffer.buffer.end(); it++) {
            log_file << "read request buffer: " << it->addr
                     << " arrival_time: " << it->arrive_global_controller << std::endl;
        }
        // std::cout<<"\n" << std::endl;
        // log_file << "\n" << std::endl;
        //print element in write request buffer
        for (auto it = write_request_buffer.buffer.begin(); it != write_request_buffer.buffer.end(); it++) {
            log_file << "write request buffer: " << it->addr
                     << " arrival_time: " << it->arrive_global_controller << std::endl;
        }
        // log_file << "\n" << std::endl;
        // print element in read request buffer
        for (auto it = m_read_in_order_q.begin(); it != m_read_in_order_q.end(); it++) {
            log_file << "read in-order request buffer: " << it->addr
                     << " arrival_time: " << it->arrive_global_controller << std::endl;
        }
        // log_file << "\n" << std::endl;
        // print element in merge queue
        for (auto it = m_receive_merge_q.begin(); it != m_receive_merge_q.end(); it++) {
            log_file << "receive merge queue: " << it->addr
                     << " arrival_time: " << it->arrive_global_controller << std::endl;
        }

        log_file << "====================" << std::endl;

        // log_file.close(); // 可省略，ofstream destructor 會自動關閉
    // }
}



  void issue() override {
    // if(write_flush_flag){
    //   std::cout << "write flush = 1" << std::endl;
    // }
    // else{
    //   std::cout << "write flush = 0" << std::endl;
    // }

    //determine read or write request buffer be issued
    if (write_flush_flag) {
      if (write_request_buffer.size() == 0) {
        write_flush_flag = false;
        // std::cout <<"m_clk: " << m_clk << " write flush = 0" << std::endl;
      }
    } 
    else {
      if (write_request_buffer.size() >= write_flush_threshold) {
        write_flush_flag = true;
        // std::cout <<"m_clk: " << m_clk<< " Write_buffer over watermark, write flush = 1" << std::endl;
      }
    }

    
    Request* req_candidate = nullptr;
    // Step 2: 選擇 read 或 write buffer 的 front 作為目標 request
    if (read_request_buffer.size() != 0 && !write_flush_flag) {
      Request& read_req = read_request_buffer.front();
      req_candidate = &read_req;
      // if (RAW_check(read_req, write_request_buffer)) {
      //   // Read-after-write 相依，觸發 write flush
      //   write_flush_flag = true;
      // } else {
      //   req_candidate = &read_req;
      // }
    }
    else if (req_candidate == nullptr && write_request_buffer.size() != 0) {
      req_candidate = &write_request_buffer.front();
    }
    else{//(req_candidate == nullptr) 
      // 沒有可發送的 request
      return;
    }

    // Step 3: 取得 channel_id 並發送 request
    if (req_candidate->addr_vec.empty()) {
      // std::cerr << "Error: addr_vec is empty for request\n";
      return;
    }

    int channel_id = req_candidate->addr_vec[0];
    // 發送並從 buffer 中移除
    bool is_success = m_controllers[channel_id]->send(*req_candidate);

    if (is_success) {
      switch (req_candidate->type_id) {
      case Request::Type::Read: {
        // std::cout <<"m_clk: " << m_clk<< " Read request "<< req_candidate->addr << " is issued to channel " << channel_id << " R_buffer_size:" << read_request_buffer.size() - 1 << " W_buffer_size: " << write_request_buffer.size() << std::endl;
        s_num_read_requests++;
        // m_logger->debug("Enqueueing read request at Clk={}, Addr={}, Type={}",
        //                 m_clk, req.addr, req.type_id);
        m_read_in_order_q.push_back(*req_candidate);
        read_request_buffer.buffer.pop_front();
        break;
      }
      case Request::Type::Write: {
        // std::cout <<"m_clk: " << m_clk<< " Write request "<< req_candidate->addr << " is issued to channel " << channel_id << " R_buffer_size:" << read_request_buffer.size()  << " W_buffer_size: " << write_request_buffer.size() - 1<< std::endl;
        s_num_write_requests++;
        // m_logger->debug("Enqueueing write request at Clk={}, Addr={}, Type={}",
        //                 m_clk, req.addr, req.type_id);
        write_request_buffer.buffer.pop_front();
        break;
      }
      default: {
        s_num_other_requests++;
        break;
      }
      }
    }
    else {
      // std::cout << "m_clk:" << m_clk << " Error: Failed to send request to channel " << channel_id << "\n";
    }

    
    // print_q_element();

    return;
  }

  // bool RAW_check(Request req, ReqBuffer &buffer){
  //   //check if the request is a read request
  //   if(req.type_id == Request::Type::Read){
  //     //check if the request has RAW dependency with the write request
  //     for(auto it = buffer.end(); it != buffer.begin(); it--){
  //       if(it->type_id == Request::Type::Write && it->addr_vec == req.addr_vec){
  //         return true;
  //       }
  //     }
  //   }
  //   return false;

  // }
  bool RAW_check(Request req, ReqBuffer &buffer) {
    if (req.type_id == Request::Type::Read) {
      for (auto it = std::prev(buffer.end()); ; --it) {
        if (it->type_id == Request::Type::Write && it->addr_vec == req.addr_vec) {
          return true;
        }
        if (it == buffer.begin()) break;
      }
    }
    return false;
  }



  //frontend send R/W request to request buffer
  bool send(Request req) override {
    req.arrive_global_controller = m_clk;

    m_addr_mapper->apply(req);
    int channel_id = req.addr_vec[0];
    bool send_is_success = false;
    //add to R/W queue
    if(write_flush_flag == false){
      if(req.type_id == Request::Type::Read){
        send_is_success = read_request_buffer.enqueue(req);
        if(send_is_success){
          // std::cout <<"m_clk: " << m_clk<< " R Request " << req.addr << " is sent to GlobalController " << " R_buffer_size:" << read_request_buffer.size()  << " W_buffer_size:" << write_request_buffer.size() << std::endl;
          if(RAW_check(req, write_request_buffer)){
            write_flush_flag = true; 
            // std::cout <<"m_clk: " << m_clk<< " RAW happen, write flush = 1" << std::endl;
          }
        }
        else{
          // std::cout <<"m_clk: " << m_clk<< " R_buffer is full, cannot enqueue R Request !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!" << std::endl;
        }
      }else if(req.type_id == Request::Type::Write){
        send_is_success = write_request_buffer.enqueue(req);
        if(send_is_success){
          // std::cout <<"m_clk: " << m_clk<< " W Request " << req.addr << " is sent to GlobalController " << " R_buffer_size:" << read_request_buffer.size() << " W_buffer_size:" << write_request_buffer.size()<< std::endl;
        }
      }else{
        throw std::runtime_error("Invalid request type!");
      }
    }
    else {
      send_is_success = false;
    }
    // print_q_element();
    return send_is_success;

    // check rd write queue, 

    // To debug: For test Case 1 @ core 36 to 1 channel, the core_id 36 is never served, never a success
    // Since core_id 36 is never served, the simulation would never completes.
    // 1. Why other cores still keep simulating even if the core_id 36 is never served?
    // 2. Why core_id 36 is never served?
    // bool is_success = m_controllers[channel_id]->send(req); // Sends the request to the controller


    // if (is_success) {
    //   switch (req.type_id) {
    //   case Request::Type::Read: {
    //     s_num_read_requests++;
    //     // m_logger->debug("Enqueueing read request at Clk={}, Addr={}, Type={}",
    //     //                 m_clk, req.addr, req.type_id);
    //     m_read_in_order_q.push_back(req);
    //     break;
    //   }
    //   case Request::Type::Write: {
    //     s_num_write_requests++;
    //     // m_logger->debug("Enqueueing write request at Clk={}, Addr={}, Type={}",
    //     //                 m_clk, req.addr, req.type_id);
    //     break;
    //   }
    //   default: {
    //     s_num_other_requests++;
    //     break;
    //   }
    //   }
    // }

    // return is_success;
  };

  bool ordered_receive(Request req) override {
    m_receive_merge_q.push_back(req);
    // print_q_element();
    return false;
  }

  void tick() override {
    m_clk++;
    m_dram->tick();

    //send request to controller
    // issue();

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
                return a.arrive_global_controller < b.arrive_global_controller;
              });

    // Traverse the whole m_receive_merge_q
    for (auto it = m_receive_merge_q.begin(); it != m_receive_merge_q.end();) {
      // check if the request is the same as the first request in the in order
      // queue
      if (m_read_in_order_q.front().arrive_global_controller == it->arrive_global_controller) {
        // std::ofstream log_file("/home/cym/ym_training/Master_Thesis_MC/ramulator2/src/memory_system/impl/log.txt", std::ios::app); // 用 append 模式寫入
        // log_file << "m_clk: " << m_clk<< " Global_Controller send read data to frontend at Clk=" << m_clk
        //               << ", Addr=" << it->addr << ", arrival_time=" << it->arrive_global_controller
        //               << std::endl;
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
          // std::cout <<"m_clk: " << m_clk<< " Global_Controller send read data to frontend at Clk=" << m_clk
          //             << ", Addr=" << req_to_callback.addr << ", Type=" << req_to_callback.type_id
          //             << std::endl;
          req_to_callback.callback(req_to_callback);
          break;
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



//Global_Controller_memory_system RAW w write buffer 
// #include "addr_mapper/addr_mapper.h"
// #include "base/request.h"
// #include "dram/dram.h"
// #include "dram_controller/controller.h"
// #include "memory_system/memory_system.h"
// #include "translation/translation.h"
// #include <deque>
// #include "dram_controller/scheduler.h"

// #include <fstream>

// namespace Ramulator {

// class Global_Controller_DRAMSystem final : public IMemorySystem, public Implementation {
//   RAMULATOR_REGISTER_IMPLEMENTATION(IMemorySystem, Global_Controller_DRAMSystem,
//                                     "Global_Controller",
//                                     "A generic DRAM-based memory system.");
//   // I suppose should reorder and ensure the data in-order property is
//   // maintained We use callbank function to inform the frontend the request has
//   // ended thus returning the control

// protected:
//   Clk_t m_clk = 0;
//   IDRAM *m_dram;
//   IAddrMapper *m_addr_mapper;
//   std::vector<IDRAMController *> m_controllers;
//   std::deque<Request> m_read_in_order_q;
//   std::deque<Request> m_receive_merge_q;

//   ReqBuffer read_request_buffer;
//   ReqBuffer write_request_buffer;
//   int write_flush_threshold; // Set the threshold for write flush
//   bool write_flush_flag; // Set the flag for write flush

//   int RAW_num = 0;
  
// public:
//   int s_num_read_requests = 0;
//   int s_num_write_requests = 0;
//   int s_num_other_requests = 0;
//   float s_average_bandwidth = 0;
//   float s_peak_bandwidth = 0;
//   float s_worst_bandwidth = 0;
//   IScheduler*   m_scheduler = nullptr;
 

// public:
//   void init() override {
//     std::ofstream clear_file("log.txt", std::ios::trunc); // trunc 會清空檔案
//     clear_file.close(); // 關閉檔案
//     //set R/W queue size
//     read_request_buffer.set_queue_size(8 - 1);
//     write_request_buffer.set_queue_size(8 - 1);
//     write_flush_flag = false;
//     write_flush_threshold = 4;

//     // m_scheduler = create_child_ifce<IScheduler>();////////


//     // Create device (a top-level node wrapping all channel nodes)
//     m_dram = create_child_ifce<IDRAM>();
//     m_addr_mapper = create_child_ifce<IAddrMapper>();

//     int num_channels = m_dram->get_level_size("channel");

//     // Create memory controllers
//     for (int i = 0; i < num_channels; i++) {
//       IDRAMController *controller = create_child_ifce<IDRAMController>();
//       controller->m_impl->set_id(fmt::format("Channel {}", i));
//       controller->m_channel_id = i;
//       m_controllers.push_back(controller);
//     }

//     m_logger = Logging::create_logger("GenericDRAMSystemLog");

//     m_clock_ratio = param<uint>("clock_ratio").required();

//     register_stat(m_clk).name("memory_system_cycles");
//     register_stat(s_num_read_requests).name("total_num_read_requests");
//     register_stat(s_num_write_requests).name("total_num_write_requests");
//     register_stat(s_num_other_requests).name("total_num_other_requests");

//     register_stat(s_average_bandwidth).name("average_bandwidth");
//     register_stat(s_peak_bandwidth).name("peak_bandwidth");
//     register_stat(s_worst_bandwidth).name("worst_bandwidth");
//   };

//   void setup(IFrontEnd *frontend, IMemorySystem *memory_system) override {}

  

//   //20250506
//   //define issue function
//   //check R/W queue when tick
//   //define a new flag for write flush
//   //write flush condition
//   //1. if the write queue length is larger than the threshold(watermark)
//   //2. if the read request has RAW dependency with the write request
//   // void issue(){
//   //   ReqBuffer::iterator req_candidate;
//   //   if(read_request_buffer.size() != 0 && write_flush_flag == false){
//   //     req_candidate = m_scheduler->get_best_request(read_request_buffer);
//   //     if(RAW_check(*req_candidate, write_request_buffer)){
//   //       //if the read request has RAW dependency with the write request
//   //       //then we need to flush the write request
//   //       write_flush_flag = true;
//   //     }
//   //     else{
//   //       // if(write_request_buffer.size() != 0){
//   //       //   req_candidate = m_scheduler->get_best_request(write_request_buffer);
//   //       // }  
//   //     }
//   //   }
//   //   else if(write_request_buffer.size() != 0){
//   //     req_candidate = m_scheduler->get_best_request(write_request_buffer);
//   //   }
//   //   else{
//   //     return;
//   //   }
//   //   //issue the request
//   //   int channel_id = req_candidate->addr_vec[0];

//   //   bool is_success = m_controllers[channel_id]->send(*req_candidate);

//   //   if (is_success) {
//   //     switch (req_candidate->type_id) {
//   //     case Request::Type::Read: {
//   //       std::cout << "Read request"<< req_candidate->addr_vec[0] << "is issued to channel " << channel_id << std::endl;
//   //       s_num_read_requests++;
//   //       // m_logger->debug("Enqueueing read request at Clk={}, Addr={}, Type={}",
//   //       //                 m_clk, req.addr, req.type_id);
//   //       m_read_in_order_q.push_back(*req_candidate);
//   //       read_request_buffer.remove(req_candidate);
//   //       break;
//   //     }
//   //     case Request::Type::Write: {
//   //       std::cout << "Write request"<< req_candidate->addr_vec[0] << "is issued to channel " << channel_id << std::endl;
//   //       s_num_write_requests++;
//   //       // m_logger->debug("Enqueueing write request at Clk={}, Addr={}, Type={}",
//   //       //                 m_clk, req.addr, req.type_id);
//   //       write_request_buffer.remove(req_candidate);
//   //       break;
//   //     }
//   //     default: {
//   //       s_num_other_requests++;
//   //       break;
//   //     }
//   //     }
//   //   }


//   //   //determine read or write request buffer be issued
//   //   if (write_flush_flag) {
//   //     if (write_request_buffer.size() == 0) {
//   //       write_flush_flag = false;
//   //     }
//   //   } else {
//   //     if (write_request_buffer.size() >= write_flush_threshold) {
//   //       write_flush_flag = true;
//   //     }
//   //   }

//   //   return;
//   // }



//   void issue(){
//     // if(write_flush_flag){
//     //   std::cout << "write flush = 1" << std::endl;
//     // }
//     // else{
//     //   std::cout << "write flush = 0" << std::endl;
//     // }

//     //determine read or write request buffer be issued
//     if (write_flush_flag) {
//       if (write_request_buffer.size() == 0) {
//         write_flush_flag = false;
//         // std::cout << " write flush = 0" << std::endl;
//       }
//     } else {
//       if (write_request_buffer.size() >= write_flush_threshold) {
//         write_flush_flag = true;
//         // std::cout <<"m_clk: " << m_clk << " Write_buffer over watermark, write flush = 1" << std::endl;
//       }
//     }

    
//     Request* req_candidate = nullptr;
//     // Step 2: 選擇 read 或 write buffer 的 front 作為目標 request
//     if (read_request_buffer.size() != 0 && !write_flush_flag) {
//       Request& read_req = read_request_buffer.front();
//       req_candidate = &read_req;
//       // if (RAW_check(read_req, write_request_buffer)) {
//       //   // Read-after-write 相依，觸發 write flush
//       //   write_flush_flag = true;
//       // } else {
//       //   req_candidate = &read_req;
//       // }
//     }
//     else if (req_candidate == nullptr && write_request_buffer.size() != 0) {
//       req_candidate = &write_request_buffer.front();
//     }
//     else{//(req_candidate == nullptr) 
//       // 沒有可發送的 request
//       return;
//     }

//     // Step 3: 取得 channel_id 並發送 request
//     if (req_candidate->addr_vec.empty()) {
//       std::cerr << "Error: addr_vec is empty for request\n";
//       return;
//     }

//     int channel_id = req_candidate->addr_vec[0];
//     // 發送並從 buffer 中移除
//     bool is_success = m_controllers[channel_id]->send(*req_candidate);

//     if (is_success) {
//       switch (req_candidate->type_id) {
//       case Request::Type::Read: {
//         // std::cout <<"m_clk: " << m_clk << " Read request "<< req_candidate->addr << " is issued to channel " << channel_id << " R_buffer_size:" << read_request_buffer.size() - 1 << " W_buffer_size: " << write_request_buffer.size() << std::endl;
//         s_num_read_requests++;
//         // m_logger->debug("Enqueueing read request at Clk={}, Addr={}, Type={}",
//         //                 m_clk, req.addr, req.type_id);

//         //20250512
//         // m_read_in_order_q.push_back(*req_candidate);

//         read_request_buffer.buffer.pop_front();
//         break;
//       }
//       case Request::Type::Write: {
//         // std::cout <<"m_clk: " << m_clk<< " Write request "<< req_candidate->addr << " is issued to channel " << channel_id << " R_buffer_size:" << read_request_buffer.size()  << " W_buffer_size: " << write_request_buffer.size() - 1<< std::endl;
//         s_num_write_requests++;
//         // m_logger->debug("Enqueueing write request at Clk={}, Addr={}, Type={}",
//         //                 m_clk, req.addr, req.type_id);
//         write_request_buffer.buffer.pop_front();
//         break;
//       }
//       default: {
//         s_num_other_requests++;
//         break;
//       }
//       }
//     }
//     else {
//       // std::cout << "m_clk:" << m_clk << " Error: Failed to send request to channel " << channel_id << "\n";
//     }
//     // print_q_element();

    
    

//     return;
//   }

//   // bool RAW_check(Request req, ReqBuffer &buffer){
//   //   //check if the request is a read request
//   //   if(req.type_id == Request::Type::Read){
//   //     //check if the request has RAW dependency with the write request
//   //     for(auto it = buffer.end(); it != buffer.begin(); it--){
//   //       if(it->type_id == Request::Type::Write && it->addr_vec == req.addr_vec){
//   //         return true;
//   //       }
//   //     }
//   //   }
//   //   return false;

//   // void print_q_element(){
//   //   if(m_clk <=40){
//   //     std::cout<< "====================" << std::endl;
//   //     std::cout << "m_clk: " << m_clk << std::endl;
//   //     //print element in read request buffer
//   //     for(auto it = m_read_in_order_q.begin(); it != m_read_in_order_q.end(); it++){
//   //       std::cout << "read in-order request buffer: " << it->addr << " arrival_time: " << it->arrive_global_controller << std::endl;
//   //     }
//   //     //print element in merge queue
//   //     for(auto it = m_receive_merge_q.begin(); it != m_receive_merge_q.end(); it++){
//   //       std::cout << "receive merge queue: " << it->addr << " arrival_time: " << it->arrive_global_controller << std::endl;
//   //     }
//   //     std::cout<< "====================" << std::endl;
//   //   }
//   // }
//   void print_q_element() {
//     // if (m_clk <= 40) {
//         std::ofstream log_file("/home/cym/ym_training/Master_Thesis_MC/ramulator2/src/memory_system/impl/log.txt", std::ios::app); // 用 append 模式寫入
//         if (!log_file.is_open()) {
//             std::cerr << "Failed to open log file." << std::endl;
//             return;
//         }
//         log_file << "====================" << std::endl;
//         log_file << "m_clk: " << m_clk << std::endl;
//         //print size of element in read request buffer
//         std::cout << "read request buffer size: " << read_request_buffer.size() << std::endl;
//         std::cout << "write request buffer size: " << write_request_buffer.size() << std::endl;
//         std::cout << "read in-order request buffer size: " << m_read_in_order_q.size() << std::endl;
//         std::cout << "receive merge queue size: " << m_receive_merge_q.size() << std::endl;

//        log_file << "read request buffer size: " << read_request_buffer.size() << std::endl;
//        log_file << "write request buffer size: " << write_request_buffer.size() << std::endl;
//        log_file << "read in-order request buffer size: " << m_read_in_order_q.size() << std::endl;
//        log_file << "receive merge queue size: " << m_receive_merge_q.size() << std::endl;
//        log_file << "\n" << std::endl;
//        //print element in read request buffer
//         for (auto it = read_request_buffer.buffer.begin(); it != read_request_buffer.buffer.end(); it++) {
//             log_file << "read request buffer: " << it->addr
//                      << " arrival_time: " << it->arrive_global_controller << std::endl;
//         }
//         // std::cout<<"\n" << std::endl;
//         // log_file << "\n" << std::endl;
//         //print element in write request buffer
//         for (auto it = write_request_buffer.buffer.begin(); it != write_request_buffer.buffer.end(); it++) {
//             log_file << "write request buffer: " << it->addr
//                      << " arrival_time: " << it->arrive_global_controller << std::endl;
//         }
//         // log_file << "\n" << std::endl;
//         // print element in read request buffer
//         for (auto it = m_read_in_order_q.begin(); it != m_read_in_order_q.end(); it++) {
//             log_file << "read in-order request buffer: " << it->addr
//                      << " arrival_time: " << it->arrive_global_controller << std::endl;
//         }
//         // log_file << "\n" << std::endl;
//         // print element in merge queue
//         for (auto it = m_receive_merge_q.begin(); it != m_receive_merge_q.end(); it++) {
//             log_file << "receive merge queue: " << it->addr
//                      << " arrival_time: " << it->arrive_global_controller << std::endl;
//         }

//         log_file << "====================" << std::endl;

//         // log_file.close(); // 可省略，ofstream destructor 會自動關閉
//     // }
// }

//   // }
//   bool RAW_check(Request req, ReqBuffer &buffer) {
//     if (req.type_id == Request::Type::Read) {
//       for (auto it = std::prev(buffer.end()); ; --it) {
//         if (it->type_id == Request::Type::Write && it->addr_vec == req.addr_vec) {
//           //write buffer write to merge queue
//           //delete read request in read request buffer
//           m_receive_merge_q.push_back(req);
//           m_read_in_order_q.push_back(req);
//           // print_q_element();
//           RAW_num++;
//           std::cout << "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!RAW_num: " << RAW_num << std::endl;
//           return true;
//         }
//         if (it == buffer.begin()) break;
//       }
//     }
//     return false;
//   }



//   //frontend send R/W request to request buffer
//   bool send(Request req) override {
//     req.arrive_global_controller = m_clk;

//     m_addr_mapper->apply(req);
//     int channel_id = req.addr_vec[0];
//     bool send_is_success = false;
//     //add to R/W queue
//     if(write_flush_flag == false){
//       if(req.type_id == Request::Type::Read){
//         // send_is_success = read_request_buffer.enqueue(req);
//         send_is_success = read_request_buffer.size() < read_request_buffer.max_size;
//         if(send_is_success){
//           // std::cout << "R Request " << req.addr << " is sent to GlobalController " << " R_buffer_size:" << read_request_buffer.size()  << " W_buffer_size:" << write_request_buffer.size() << std::endl;
//           if(RAW_check(req, write_request_buffer)){
//             // write_flush_flag = true;
//             // std::cout << "RAW happen, write flush = 1" << std::endl;
//             // std::cout <<"m_clk: " << m_clk<< " RAW happen,write req to merge buffer"<< " ,arrival_time: " << req.arrive_global_controller << std::endl;
//           }
//           else{
//             //20250512
//             m_read_in_order_q.push_back(req);

//             send_is_success = read_request_buffer.enqueue(req);
//             // std::cout <<"m_clk: " << m_clk<< " R Request " << req.addr << " is sent to GlobalController " << " R_buffer_size:" << read_request_buffer.size()  << " W_buffer_size:" << write_request_buffer.size() << std::endl;
//           }
//           // std::cout << "R Request " << req.addr << " is sent to GlobalController " << " R_buffer_size:" << read_request_buffer.size()  << " W_buffer_size:" << write_request_buffer.size() << std::endl;
//         }
//         else{
//           // std::cout <<"m_clk: " << m_clk<< " R_buffer is full, cannot enqueue R Request !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!" << std::endl;
//         }
//       }else if(req.type_id == Request::Type::Write){
//         send_is_success = write_request_buffer.enqueue(req);
//         if(send_is_success){
//           // std::cout <<"m_clk: " << m_clk<< " W Request " << req.addr << " is sent to GlobalController " << " R_buffer_size:" << read_request_buffer.size() << " W_buffer_size:" << write_request_buffer.size()<< std::endl;
//         }
//       }else{
//         throw std::runtime_error("Invalid request type!");
//       }
//     }
//     else {
//       send_is_success = false;
//     }
//     // print_q_element();
//     return send_is_success;

//     // check rd write queue, 

//     // To debug: For test Case 1 @ core 36 to 1 channel, the core_id 36 is never served, never a success
//     // Since core_id 36 is never served, the simulation would never completes.
//     // 1. Why other cores still keep simulating even if the core_id 36 is never served?
//     // 2. Why core_id 36 is never served?
//     // bool is_success = m_controllers[channel_id]->send(req); // Sends the request to the controller


//     // if (is_success) {
//     //   switch (req.type_id) {
//     //   case Request::Type::Read: {
//     //     s_num_read_requests++;
//     //     // m_logger->debug("Enqueueing read request at Clk={}, Addr={}, Type={}",
//     //     //                 m_clk, req.addr, req.type_id);
//     //     m_read_in_order_q.push_back(req);
//     //     break;
//     //   }
//     //   case Request::Type::Write: {
//     //     s_num_write_requests++;
//     //     // m_logger->debug("Enqueueing write request at Clk={}, Addr={}, Type={}",
//     //     //                 m_clk, req.addr, req.type_id);
//     //     break;
//     //   }
//     //   default: {
//     //     s_num_other_requests++;
//     //     break;
//     //   }
//     //   }
//     // }

//     // return is_success;
//   };

//   bool ordered_receive(Request req) override {
//     m_receive_merge_q.push_back(req);
//     // print_q_element();
//     return false;
//   }

//   void tick() override {
//     m_clk++;
//     m_dram->tick();

//     //send request to controller
//     // issue();

//     for (auto controller : m_controllers) {
//       controller->tick(); // Tick the controller
//     }
//     in_order_callback();
//   };

//   float get_tCK() override { return m_dram->m_timing_vals("tCK_ps") / 1000.0f; }

// private:
//   void in_order_callback() {
//     // First check if the receive merge queue is empty
//     if (m_receive_merge_q.empty()) {
//       return;
//     }

//     // reorder the merge queue with the arrival time
//     std::sort(m_receive_merge_q.begin(), m_receive_merge_q.end(),
//               [](const Request &a, const Request &b) {
//                 return a.arrive_global_controller < b.arrive_global_controller;
//               });        
//     // print_q_element();          
//     // Traverse the whole m_receive_merge_q
//     for (auto it = m_receive_merge_q.begin(); it != m_receive_merge_q.end();) {
//       // check if the request is the same as the first request in the in order
//       // queue
//       if (m_read_in_order_q.front().arrive_global_controller == it->arrive_global_controller) {
//         // std::ofstream log_file("/home/cym/ym_training/Master_Thesis_MC/ramulator2/src/memory_system/impl/log.txt", std::ios::app); // 用 append 模式寫入
//         // log_file << "m_clk: " << m_clk<< " Global_Controller send read data to frontend at Clk=" << m_clk
//         //               << ", Addr=" << it->addr << ", arrival_time=" << it->arrive_global_controller
//         //               << std::endl;
//         // If the request is the same as the first request in the in order queue
//         // then we can send the request to the frontend
//         // m_logger->debug(
//         //     "Sending request to frontend at Clk={}, Addr={}, Type={}", m_clk,
//         //     it->addr, it->type_id);

//         Request req_to_callback = m_read_in_order_q.front();

//         it = m_receive_merge_q.erase(it);
//         m_read_in_order_q.pop_front();

//         // TO-DO Callback to the frontend if it has a callback to do
//         if (req_to_callback.callback != nullptr) {
//           // std::cout <<"m_clk: " << m_clk<< " Global_Controller send read data to frontend at Clk=" << m_clk
//           //             << ", Addr=" << req_to_callback.addr << ", Type=" << req_to_callback.type_id
//           //             << std::endl;
//           req_to_callback.callback(req_to_callback);
//         }
//         break;//20250512
//       } else {
//         // If the request is not the same as the first request in the in order
//         // queue then we can break the loop
//         break;
//       }
//     }
//   }

//   // const SpecDef& get_supported_requests() override {
//   //   return m_dram->m_requests;
//   // };
// };

// } // namespace Ramulator
