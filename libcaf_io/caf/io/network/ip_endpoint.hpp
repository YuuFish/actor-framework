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

#ifndef CAF_IO_IP_ENDPOINT_HPP
#define CAF_IO_IP_ENDPOINT_HPP

#include <deque>
#include <vector>
#include <cstring>
#include <functional>

#ifdef CAF_WINDOWS
# include <windows.h>
# include <winsock2.h>
# include <ws2tcpip.h>
# include <ws2ipdef.h>
#else
# include <unistd.h>
# include <cerrno>
# include <arpa/inet.h>
# include <sys/socket.h>
# include <netinet/in.h>
# include <netinet/ip.h>
#endif

#include "caf/meta/type_name.hpp"

namespace {

template <int Bits>
struct hash_conf {
  static_assert(Bits != 4 || Bits != 8, "Unspported hash length.");
  static constexpr uint32_t basis = 0;
  static constexpr uint32_t prime = 0;
};

template <>
struct hash_conf<4> {
  static constexpr uint32_t basis = 2166136261u;
  static constexpr uint32_t prime = 16777619u;
};

template <>
struct hash_conf<8> {
  static constexpr uint64_t basis = 14695981039346656037u;
  static constexpr uint64_t prime = 1099511628211u;
};

constexpr hash_conf<sizeof(size_t)> conf;

constexpr uint8_t byte_at(int i);
constexpr size_t prehash(int i);
constexpr size_t prehash();

} // namespace <anonymous>

namespace caf {
namespace io {
namespace network {

// hash for char*, see:
// - https://en.wikipedia.org/wiki/Fowler%E2%80%93Noll%E2%80%93Vo_hash_function
// - http://www.isthe.com/chongo/tech/comp/fnv/index.html
// Always hash 128 bit address, for v4 we use the embedded addr.
// TODO: keep port separately or use a trie instead of a hashmap?
class ip_hash {
public:
  ip_hash();
  size_t operator()(const struct sockaddr_storage& sa) const noexcept;
  size_t hash(const struct sockaddr_in* sa) const noexcept;
  size_t hash(const struct sockaddr_in6* sa) const noexcept;
};

struct ip_endpoint {
  struct sockaddr_storage addr;
  socklen_t len;
};

bool operator==(const ip_endpoint& lhs, const ip_endpoint& rhs);

template <class Inspector>
typename Inspector::result_type inspect(Inspector& f, ip_endpoint& ep) {
  auto& sa = ep.addr;
  uint16_t port = 0;
  char addr[INET6_ADDRSTRLEN];
  switch(sa.ss_family) {
    case AF_INET:
      port = ntohs(reinterpret_cast<const sockaddr_in*>(&sa)->sin_port);
      inet_ntop(AF_INET,
                &reinterpret_cast<const sockaddr_in*>(&sa)->sin_addr,
                addr, INET_ADDRSTRLEN);
      break;
    case AF_INET6:
      port = ntohs(reinterpret_cast<const sockaddr_in6*>(&sa)->sin6_port);
      inet_ntop(AF_INET6,
                &reinterpret_cast<const sockaddr_in*>(&sa)->sin_addr,
                addr, INET6_ADDRSTRLEN);
      break;
    default:
      addr[0] = '\0';
      break;
  }
  return f(meta::type_name("ip_endpoint"), addr, port, ep.len);
}

// TODO: write inspector?

} // namespace network
} // namespace io
} // namespace caf

namespace std {

template <>
struct hash<caf::io::network::ip_endpoint> {
  using argument_type = caf::io::network::ip_endpoint;
  using result_type = size_t;
  result_type operator()(const argument_type& ep) const {
    return caf::io::network::ip_hash{}(ep.addr);
  }
};

} // namespace std


#endif // CAF_IO_IP_ENDPOINT_HPP
