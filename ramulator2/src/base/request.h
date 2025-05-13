#ifndef     RAMULATOR_BASE_REQUEST_H
#define     RAMULATOR_BASE_REQUEST_H

#include <vector>
#include <list>
#include <string>

#include "base/base.h"

namespace Ramulator {

struct Request {
  Addr_t    addr = -1;
  AddrVec_t addr_vec {};

  // Basic request id convention
  // 0 = Read, 1 = Write. The device spec defines all others
  struct Type {
    enum : int {
      Read = 0,
      Write,
    };
  };

  int type_id = -1;    // An identifier for the type of the request
  int source_id = -1;  // An identifier for where the request is coming from (e.g., which core)

  int command = -1;          // The command that need to be issued to progress the request
  int final_command = -1;    // The final command that is needed to finish the request
  bool is_stat_updated = false; // Memory controller stats

  Clk_t arrive = -1;   // Clock cycle when the request arrive at the memory controller
  Clk_t depart = -1;   // Clock cycle when the request depart the memory controller
  Clk_t arrive_global_controller = -1; // Clock cycle when the request arrive at the global controller

  Clk_t request_issue_delay = -1;

  std::array<int, 4> scratchpad = { 0 };    // A scratchpad for the request

  std::function<void(Request&)> callback; // A callback function for the request

  void* m_payload = nullptr;    // Point to a generic payload

  Request(Addr_t addr, int type);
  Request(AddrVec_t addr_vec, int type);
  Request(Addr_t addr, int type, int source_id, std::function<void(Request&)> callback);
  Request(Addr_t addr, int type, std::function<void(Request&)> callback);
};


struct ReqBuffer {
  std::list<Request> buffer;
  size_t max_size = 1;


  using iterator = std::list<Request>::iterator;
  iterator begin() { return buffer.begin(); };
  iterator end() { return buffer.end(); };

  void set_queue_size(size_t size) {
    max_size = size;
  }

  size_t size() const { return buffer.size(); }

  bool enqueue(const Request& request) {
    if (buffer.size() <= max_size) {
      buffer.push_back(request);
      return true;
    } else {
      return false;
    }
  }

  void remove(iterator it) {
    buffer.erase(it);
  }


  //jimmy add in 20250501
  // --- Front / Back 元素操作 ---

  // ✅ 回傳 front 的參考（可修改）
  Request& front() {
    return buffer.front();
  }

  const Request& front() const {
    return buffer.front();
  }

  // ✅ 回傳 back 的參考（可修改）
  Request& back() {
    return buffer.back();
  }

  const Request& back() const {
    return buffer.back();
  }

  // ✅ 若想回傳複製值（by value）
  Request get_front() const {
    return buffer.front();
  }

  Request get_back() const {
    return buffer.back();
  }

  // ✅ 移除 front
  void pop_front() {
    if (!buffer.empty()) {
      buffer.pop_front();
    }
  }

  // ✅ 移除 back（若需要）
  void pop_back() {
    if (!buffer.empty()) {
      buffer.pop_back();
    }
  }
};






}        // namespace Ramulator


#endif   // RAMULATOR_BASE_REQUEST_H