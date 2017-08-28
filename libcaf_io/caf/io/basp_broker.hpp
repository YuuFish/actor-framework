/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2017                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#ifndef CAF_IO_BASP_BROKER_HPP
#define CAF_IO_BASP_BROKER_HPP

#include <map>
#include <set>
#include <string>
#include <future>
#include <vector>
#include <unordered_map>
#include <unordered_set>

#include "caf/stateful_actor.hpp"
#include "caf/proxy_registry.hpp"
#include "caf/binary_serializer.hpp"
#include "caf/binary_deserializer.hpp"
#include "caf/forwarding_actor_proxy.hpp"

#include "caf/io/basp/all.hpp"
#include "caf/io/broker.hpp"
#include "caf/io/visitors.hpp"
#include "caf/io/typed_broker.hpp"
#include "caf/io/endpoint_context.hpp"

namespace caf {
namespace io {

struct basp_broker_state : proxy_registry::backend, basp::instance::callee {
  basp_broker_state(broker* selfptr);

  ~basp_broker_state() override;

  // inherited from proxy_registry::backend
  strong_actor_ptr make_proxy(node_id nid, actor_id aid) override;

  // inherited from proxy_registry::backend
  execution_unit* registry_context() override;

  // inherited from basp::instance::listener
  void finalize_handshake(const node_id& nid, actor_id aid,
                          std::set<std::string>& sigs) override;

  // inherited from basp::instance::listener
  void purge_state(const node_id& nid) override;

  void purge(connection_handle hdl);
  void purge(dgram_handle hdl);

  // inherited from basp::instance::listener
  void proxy_announced(const node_id& nid, actor_id aid) override;

  // inherited from basp::instance::listener
  void deliver(const node_id& src_nid, actor_id src_aid,
               actor_id dest_aid, message_id mid,
               std::vector<strong_actor_ptr>& stages, message& msg) override;

  // inherited from basp::instance::listener
  void deliver(const node_id& src_nid, actor_id src_aid,
               atom_value dest_name, message_id mid,
               std::vector<strong_actor_ptr>& stages, message& msg) override;

  // called from both overriden functions
  void deliver(const node_id& src_nid, actor_id src_aid,
               strong_actor_ptr dest, message_id mid,
               std::vector<strong_actor_ptr>& stages, message& msg);

  // performs bookkeeping such as managing `spawn_servers`
  void learned_new_node(const node_id& nid);

  // inherited from basp::instance::listener
  void learned_new_node_directly(const node_id& nid,
                                 bool was_indirectly_before) override;

  // inherited from basp::instance::listener
  void learned_new_node_indirectly(const node_id& nid) override;

  // inherited from basp::instance::listener
  uint16_t next_sequence_number(connection_handle hdl) override;

  // inherited from basp::instance::listener
  uint16_t next_sequence_number(dgram_handle hdl) override;

  // inherited from basp::instance::listener
  void add_pending(uint16_t seq, endpoint_context& ep, basp::header hdr,
                   std::vector<char> payload) override;

  // inherited from basp::instance::listener
  bool deliver_pending(execution_unit* ctx, endpoint_context& ep) override;

  void handle_heartbeat(const node_id&) override {
    // nop
  }

  void set_context(connection_handle hdl);
  void set_context(dgram_handle hdl);

  // visitors to handle variants
  struct purge_visitor {
    using result_type = void;
    purge_visitor(basp_broker_state* ptr) : state{ptr} { }
    template <class T>
    result_type operator()(const T& hdl) {
      return state->purge(hdl);
    }
    basp_broker_state* state;
  };
  struct sequence_number_visitor {
    using result_type = uint16_t;
    sequence_number_visitor(basp_broker_state* ptr) : state{ptr} { }
    template <class T>
    result_type operator()(const T& hdl) {
      return state->next_sequence_number(hdl);
    }
    basp_broker_state* state;
  };
  struct close_visitor {
    using result_type = void;
    close_visitor(broker* b) : b{b} { }
    template <class T>
    result_type operator()(const T& hdl) {
      b->close(hdl);
    }
    broker* b;
  };
  close_visitor close_vis;
  wr_buf_visitor wr_buf_vis;
  purge_visitor purge_state_vis;
  sequence_number_visitor seq_num_vis;

  // pointer to ourselves
  broker* self;

  // protocol instance of BASP
  basp::instance instance;

  using ctx_tcp_map = std::unordered_map<connection_handle, endpoint_context>;
  using ctx_udp_map = std::unordered_map<dgram_handle, endpoint_context>;

  // keeps context information for all open connections
  ctx_tcp_map ctx_tcp;
  ctx_udp_map ctx_udp;

  // points to the current context for callbacks such as `make_proxy`
  endpoint_context* this_context = nullptr;

  // stores handles to spawn servers for other nodes; these servers
  // are spawned whenever the broker learns a new node ID and try to
  // get a 'SpawnServ' instance on the remote side
  std::unordered_map<node_id, actor> spawn_servers;

  // can be enabled by the user to let CAF automatically try
  // to establish new connections at runtime to optimize
  // routing paths by forming a mesh between all nodes
  bool enable_automatic_connections = false;

  // returns the node identifier of the underlying BASP instance
  const node_id& this_node() const {
    return instance.this_node();
  }

  static const char* name;
};

/// A broker implementation for the Binary Actor System Protocol (BASP).
class basp_broker : public stateful_actor<basp_broker_state, broker> {
public:
  using super = stateful_actor<basp_broker_state, broker>;

  explicit basp_broker(actor_config& cfg);

  behavior make_behavior() override;
  proxy_registry* proxy_registry_ptr() override;
  resume_result resume(execution_unit*, size_t) override;

private:
  addr_visitor addr_;
  port_visitor port_;
  wr_buf_visitor wr_buf_;
};

} // namespace io
} // namespace caf

#endif // CAF_IO_BASP_BROKER_HPP
