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

#include "caf/io/network/ip_endpoint.hpp"

#include "caf/logger.hpp"

namespace {

constexpr uint8_t byte_at(int i) {
  // std::array<uint8_t, 12> static_bytes{
  //   {0x00, 0x00, 0x00, 0x00,
  //    0x00, 0x00, 0x00, 0x00,
  //    0x00, 0x00, 0xFF, 0xFF}
  // };
  return i > 11
    ? throw std::exception() // the embedded prefix is only 12 bytes long
    : i < 10 ? 0x00 : 0xFF;
}

constexpr size_t prehash(int i) {
  return (i > 0)
    ? (prehash(i - 1) * conf.prime) ^ byte_at(i)
    : (conf.basis * conf.prime) ^ byte_at(i);
}

constexpr size_t prehash() {
  // 12 bytes, so pos 11 to 0
  return prehash(11);
}

} // namespace <anonymous>

namespace caf {
namespace io {
namespace network {

ip_hash::ip_hash() {
  // nop
}

size_t ip_hash::operator()(const struct sockaddr_storage& sa) const noexcept {
  switch (sa.ss_family) {
    case AF_INET:
      return hash(reinterpret_cast<const struct sockaddr_in*>(&sa));
    case AF_INET6:
      return hash(reinterpret_cast<const struct sockaddr_in6*>(&sa));
    default:
      CAF_LOG_ERROR("Only IPv4 and IPv6 are supported.");
      return 0;
  }
}

size_t ip_hash::hash(const struct sockaddr_in* sa) const noexcept {
  auto& addr = sa->sin_addr;
  size_t res = prehash();
  // the first loop was replaces with `constexpr size_t prehash()`
  //size_t res = conf_.basis;
  //for (int i = 0; i < 12; ++i) {
    //res = res * conf_.prime;
    //res = res ^ ipv4_embedded_prefix_[i];
  //}
  for (int i = 0; i < 4; ++i) {
    res = res * conf.prime;
    res = res ^ ((addr.s_addr >> i) & 0xFF);
  }
  // TODO: separate address and port ?
  res = res * conf.prime;
  res = res ^ (sa->sin_port >> 1);
  res = res * conf.prime;
  res = res ^ (sa->sin_port & 0xFF);
  return res;
}

size_t ip_hash::hash(const struct sockaddr_in6* sa) const noexcept {
  auto& addr = sa->sin6_addr;
  size_t res = conf.basis;
  for (int i = 0; i < 16; ++i) {
    res = res * conf.prime;
    res = res ^ addr.s6_addr[i];
  }
  // TODO: separate address and port ?
  res = res * conf.prime;
  res = res ^ (sa->sin6_port >> 1);
  res = res * conf.prime;
  res = res ^ (sa->sin6_port & 0xFF);
  return res;
}

bool operator==(const ip_endpoint& lhs, const ip_endpoint& rhs) {
  if (lhs.len == rhs.len) // && lhs.addr.ss_family == rhs.addr.ss_family)
    return 0 == std::memcmp(&lhs.addr, &rhs.addr, lhs.len);
  return false;
}


} // namespace network
} // namespace io
} // namespace caf
