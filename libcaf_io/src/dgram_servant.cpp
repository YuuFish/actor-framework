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

#include "caf/io/dgram_servant.hpp"

#include "caf/logger.hpp"

namespace caf {
namespace io {

dgram_servant::dgram_servant(dgram_handle hdl) : dgram_servant_base(hdl) {
  // nop
}

dgram_servant::~dgram_servant() {
  // nop
}

message dgram_servant::detach_message() {
  // TODO: Add endpoint ID to handle
  return make_message(dgram_servant_closed_msg{hdl()});
}

bool dgram_servant::consume(execution_unit* ctx, std::vector<char>& buf) {
  // TODO: add endpoint id to handle
  // TODO: change signature to use vector<char> for passing the buffer!
  CAF_ASSERT(ctx != nullptr);
  CAF_LOG_TRACE(CAF_ARG(buf.size()));
  if (detached())
    // we are already disconnected from the broker while the multiplexer
    // did not yet remove the socket, this can happen if an I/O event causes
    // the broker to call close_all() while the pollset contained
    // further activities for the broker
    return false;
  // keep a strong reference to our parent until we leave scope
  // to avoid UB when becoming detached during invocation
  /*
  auto guard = parent_;
  auto& buf = rd_buf();
  CAF_ASSERT(buf.size() >= num);
  // make sure size is correct, swap into message, and then call client
  buf.resize(num);
  auto& msg_buf = msg().buf;
  msg_buf.swap(buf);
  auto result = invoke_mailbox_element(ctx);
  // swap buffer back to stream and implicitly flush wr_buf()
  msg_buf.swap(buf);
  flush();
  return result;
  */
  auto guard = parent_;
  auto& msg_buf = msg().buf;
  msg_buf.swap(buf);
  auto result = invoke_mailbox_element(ctx);
  // swap buffer back to stream and implicitly flush wr_buf()
  msg_buf.swap(buf);
  flush();
  return result;
}

void dgram_servant::datagram_sent(execution_unit* ctx, size_t written) {
  // TODO: add endpoint id to handle
  CAF_LOG_TRACE(CAF_ARG(written));
  if (detached())
    return;
  using sent_t = datagram_sent_msg;
  using tmp_t = mailbox_element_vals<datagram_sent_msg>;
  tmp_t tmp{strong_actor_ptr{}, message_id::make(),
            mailbox_element::forwarding_stack{},
            sent_t{hdl(), written}};
  invoke_mailbox_element_impl(ctx, tmp);
}

void dgram_servant::io_failure(execution_unit* ctx, network::operation op) {
  CAF_LOG_TRACE(CAF_ARG(hdl()) << CAF_ARG(op));
  // keep compiler happy when compiling w/o logging
  static_cast<void>(op);
  detach(ctx, true);
}

} // namespace io
} // namespace caf

