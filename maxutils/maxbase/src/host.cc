/*
 * Copyright (c) 2019 MariaDB Corporation Ab
 * Copyright (c) 2023 MariaDB plc, Finnish Branch
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file and at www.mariadb.com/bsl11.
 *
 * Change Date: 2027-09-09
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2 or later of the General
 * Public License.
 */
#include <maxbase/host.hh>

#include <ostream>
#include <vector>
#include <algorithm>
#include <array>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/un.h>
#include <maxbase/assert.hh>
#include <maxbase/format.hh>
#include <maxbase/string.hh>
#include <maxbase/lru_cache.hh>
#include <maxbase/checksum.hh>
#include <maxbase/proxy_protocol.hh>

using namespace std::chrono_literals;

namespace
{
class HostKey
{
public:
    HostKey(const sockaddr_storage& sock)
    {
        if (sock.ss_family == AF_INET)
        {
            const auto* ipv4 = reinterpret_cast<const sockaddr_in*>(&sock);
            m_size = sizeof(ipv4->sin_addr);
            memcpy(m_data.data(), &ipv4->sin_addr, m_size);
        }
        else if (sock.ss_family == AF_INET6)
        {
            const auto* ipv6 = reinterpret_cast<const sockaddr_in6*>(&sock);
            m_size = sizeof(ipv6->sin6_addr);
            memcpy(m_data.data(), &ipv6->sin6_addr, m_size);
        }
        else
        {
            mxb_assert(!true);
        }
    }

    const void* data() const
    {
        return m_data.data();
    }

    size_t size() const
    {
        return m_size;
    }

    bool operator==(const HostKey& other) const
    {
        return size() == other.size() && memcmp(data(), other.data(), size()) == 0;
    }

private:
    std::array<uint8_t, sizeof(struct in6_addr)> m_data {0};
    uint8_t                                      m_size {0};

    static_assert(sizeof(m_data) == std::max(sizeof(struct in_addr), sizeof(struct in6_addr)));
    static_assert(sizeof(m_data) < std::numeric_limits<uint8_t>::max());
};

struct HostEntry
{
    HostEntry(const std::string& result)
        : hostname(result)
        , last_updated(mxb::Clock::now())
    {
    }

    std::string            hostname;
    mxb::Clock::time_point last_updated;
};

struct ThisThread
{
    // Total time spent in name server lookups
    mxb::Duration dns_time;
    mxb::Duration rdns_time;

    mxb::lru_cache<HostKey, HostEntry, mxb::xxHasher> host_cache;
};

thread_local ThisThread this_thread;
}


// Simple but not exhaustive address validation functions.
// An ipv4 address "x.x.x.x" cannot be a hostname (where x is a number), but pretty
// much anything else can. Call is_valid_hostname() last.
bool mxb::Host::is_valid_ipv4(const std::string& ip)
{
    bool ret = ip.find_first_not_of("0123456789.") == std::string::npos
        && (ip.length() <= 15 && ip.length() >= 7)
        && std::count(begin(ip), end(ip), '.') == 3;

    return ret;
}

bool mxb::Host::is_valid_ipv6(const std::string& ip)
{
    auto invalid_char = [](char ch) {
            bool valid = std::isxdigit(ch) || ch == ':' || ch == '.';
            return !valid;
        };

    bool ret = std::count(begin(ip), end(ip), ':') >= 2
        && std::none_of(begin(ip), end(ip), invalid_char)
        && (ip.length() <= 45 && ip.length() >= 2);

    return ret;
}

namespace
{

bool is_valid_hostname(const std::string& hn)
{
    auto invalid_char = [](char ch) {
            bool valid = std::isalnum(ch) || ch == '_' || ch == '.' || ch == '-';
            return !valid;
        };

    bool ret = std::none_of(begin(hn), end(hn), invalid_char)
        && hn.front() != '_'
        && hn.front() != '-'
        && (hn.length() <= 253 && hn.length() > 0)
        && hn.find("--") == std::string::npos;

    return ret;
}

bool is_valid_socket(const std::string& addr)
{
    // Can't check the file system, the socket may not have been created yet.
    // Just not bothering to check much, file names can be almost anything and errors are easy to spot.
    bool ret = addr.front() == '/'
        && addr.back() != '/';      // avoids the confusing error: Address already in use

    return ret;
}

bool is_valid_port(int port)
{
    return 0 < port && port < (1 << 16);
}

// Make sure the order here is the same as in Host::Type.
static std::vector<std::string> host_type_names = {"Invalid", "UnixDomainSocket", "HostName", "IPV4", "IPV6"};
}

namespace maxbase
{

std::string to_string(Host::Type type)
{
    size_t i = size_t(type);
    return i >= host_type_names.size() ? "UNKNOWN" : host_type_names[i];
}

void Host::set_type()
{
    if (is_valid_socket(m_address))
    {
        if (!is_valid_port(m_port))
        {
            m_type = Type::UnixDomainSocket;
        }
    }
    else if (is_valid_port(m_port))
    {
        if (is_valid_ipv4(m_address))
        {
            m_type = Type::IPV4;
        }
        else if (is_valid_ipv6(m_address))
        {
            m_type = Type::IPV6;
        }
        else if (is_valid_hostname(m_address))
        {
            m_type = Type::HostName;
        }
    }
}

Host Host::from_string(const std::string& in, int default_port)
{
    std::string input = maxbase::trimmed_copy(in);

    if (input.empty())
    {
        return Host();
    }

    std::string port_part;
    std::string address_part;

    // 'ite' is left pointing into the input if there is an error in parsing. Not exhaustive error checking.
    auto ite = input.begin();

    if (*ite == '[')
    {   // expecting [address]:port, where :port is optional
        auto last = std::find(begin(input), end(input), ']');
        std::copy(++ite, last, std::back_inserter(address_part));
        if (last != end(input))
        {
            if (++last != end(input) && *last == ':' && last + 1 != end(input))
            {
                ++last;
                std::copy(last, end(input), std::back_inserter(port_part));
                last = end(input);
            }
            ite = last;
        }
    }
    else
    {
        if (is_valid_ipv6(input))
        {
            address_part = input;
            ite = end(input);
        }
        else
        {
            // expecting address:port, where :port is optional => (hostnames with colons must use [xxx]:port)
            auto colon = std::find(begin(input), end(input), ':');
            std::copy(begin(input), colon, std::back_inserter(address_part));
            ite = colon;
            if (colon != end(input) && ++colon != end(input))
            {
                std::copy(colon, end(input), std::back_inserter(port_part));
                ite = end(input);
            }
        }
    }

    int port = default_port;
    if (ite == end(input))      // if all input consumed
    {
        if (!port_part.empty())
        {
            bool all_digits = std::all_of(begin(port_part), end(port_part),
                                          [](char ch) {
                                              return std::isdigit(ch);
                                          });
            port = all_digits ? std::atoi(port_part.c_str()) : InvalidPort;
        }
    }

    Host ret;
    ret.m_org_input = in;
    ret.m_address = address_part;
    ret.m_port = port;
    ret.set_type();

    return ret;
}

Host::Host(const std::string& addr, int port)
{
    m_org_input = addr;
    m_address = addr;
    m_port = port;

    if (!m_address.empty() && m_address.front() != '[')
    {
        set_type();
    }
}

std::ostream& operator<<(std::ostream& os, const Host& host)
{
    switch (host.type())
    {
    case Host::Type::Invalid:
        os << "INVALID input: '" << host.org_input() << "' parsed to "
           << host.address() << ":" << host.port();
        break;

    case Host::Type::UnixDomainSocket:
        os << host.address();
        break;

    case Host::Type::HostName:
    case Host::Type::IPV4:
        os << host.address() << ':' << host.port();
        break;

    case Host::Type::IPV6:
        os << '[' << host.address() << "]:" << host.port();
        break;
    }
    return os;
}

std::istream& operator>>(std::istream& is, Host& host)
{
    std::string input;
    is >> input;
    host = Host::from_string(input);
    return is;
}

bool name_lookup(const std::string& host,
                 std::unordered_set<std::string>* addresses_out,
                 std::string* error_out)
{
    addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;    /* Return any address type */
    hints.ai_socktype = SOCK_DGRAM; /* Datagram socket */
    hints.ai_flags = 0;             /* Mapped IPv4 */
    hints.ai_protocol = 0;          /* Any protocol */
    hints.ai_canonname = nullptr;
    hints.ai_addr = nullptr;
    hints.ai_next = nullptr;

    addrinfo* results = nullptr;
    bool success = false;
    std::string error_msg;

    mxb::StopWatch stopwatch;
    int rv_addrinfo = getaddrinfo(host.c_str(), nullptr, &hints, &results);
    this_thread.dns_time += stopwatch.split();

    if (rv_addrinfo == 0)
    {
        // getaddrinfo may return multiple result addresses. Save all of them.
        for (auto iter = results; iter; iter = iter->ai_next)
        {
            int address_family = iter->ai_family;
            void* addr = nullptr;
            char buf[INET6_ADDRSTRLEN];     // Enough for both address types
            if (iter->ai_family == AF_INET)
            {
                auto sa_in = (sockaddr_in*)(iter->ai_addr);
                addr = &sa_in->sin_addr;
            }
            else if (iter->ai_family == AF_INET6)
            {
                auto sa_in = (sockaddr_in6*)(iter->ai_addr);
                addr = &sa_in->sin6_addr;
            }

            inet_ntop(address_family, addr, buf, sizeof(buf));
            if (buf[0])
            {
                addresses_out->insert(buf);
                success = true;     // One successful lookup is enough.
            }
        }
        freeaddrinfo(results);
    }
    else
    {
        error_msg = mxb::string_printf("getaddrinfo() failed: '%s'.", gai_strerror(rv_addrinfo));
    }

    if (error_out)
    {
        *error_out = error_msg;
    }
    return success;
}

std::string ntop(const sockaddr* addr)
{
    std::string rval;
    void* in_addr = nullptr;
    int addr_fam = addr->sa_family;
    if (addr_fam == AF_INET)
    {
        in_addr = &((sockaddr_in*)addr)->sin_addr;
    }
    else if (addr_fam == AF_INET6)
    {
        in_addr = &((sockaddr_in6*)addr)->sin6_addr;
    }
    if (in_addr)
    {
        char buf[INET6_ADDRSTRLEN];
        inet_ntop(addr_fam, in_addr, buf, sizeof(buf));
        rval = buf;
    }
    return rval;
}

sockaddr_storage sockaddr_to_storage(const sockaddr* addr)
{
    sockaddr_storage rval {};

    switch (addr->sa_family)
    {
    case AF_INET:
        memcpy(&rval, addr, sizeof(sockaddr_in));
        break;

    case AF_INET6:
        memcpy(&rval, addr, sizeof(sockaddr_in6));
        break;

    case AF_UNIX:
        memcpy(&rval, addr, sizeof(sockaddr_un));
        break;

    default:
        break;
    }
    return rval;
}

void normalize_and_extract_remote(const sockaddr_storage& addr, sockaddr_storage* sa_dst, char* remote_dst)
{
    mxb::get_normalized_ip(addr, sa_dst);
    void* ptr = nullptr;

    if (sa_dst->ss_family == AF_INET)
    {
        ptr = &((sockaddr_in*)sa_dst)->sin_addr;
    }
    else if (sa_dst->ss_family == AF_INET6)
    {
        ptr = &((sockaddr_in6*)sa_dst)->sin6_addr;
    }

    if (ptr)
    {
        inet_ntop(sa_dst->ss_family, ptr, remote_dst, INET6_ADDRSTRLEN);
    }
    else
    {
        strcpy(remote_dst, "localhost");
    }
}

int extract_port(const sockaddr_storage& sa)
{
    if (sa.ss_family == AF_INET)
    {
        return ntohs(((sockaddr_in*)&sa)->sin_port);
    }
    else if (sa.ss_family == AF_INET6)
    {
        return ntohs(((sockaddr_in6*)&sa)->sin6_port);
    }
    return -1;
}

bool reverse_name_lookup(const std::string& ip, std::string* output, size_t max_size)
{
    sockaddr_storage socket_address;
    memset(&socket_address, 0, sizeof(socket_address));
    socklen_t slen = 0;

    if (Host::is_valid_ipv4(ip))
    {
        // Casts between the different sockaddr-types should work.
        int family = AF_INET;
        auto sa_in = reinterpret_cast<sockaddr_in*>(&socket_address);
        if (inet_pton(family, ip.c_str(), &sa_in->sin_addr) == 1)
        {
            sa_in->sin_family = family;
            slen = sizeof(sockaddr_in);
        }
    }
    else if (Host::is_valid_ipv6(ip))
    {
        int family = AF_INET6;
        auto sa_in6 = reinterpret_cast<sockaddr_in6*>(&socket_address);
        if (inet_pton(family, ip.c_str(), &sa_in6->sin6_addr) == 1)
        {
            sa_in6->sin6_family = family;
            slen = sizeof(sockaddr_in6);
        }
    }

    bool success = false;
    if (slen > 0)
    {
        if (max_size)
        {
            if (auto cached = get_cached_hostname(&socket_address))
            {
                *output = std::move(*cached);
                return true;
            }
        }

        char host[NI_MAXHOST];
        auto sa = reinterpret_cast<sockaddr*>(&socket_address);
        mxb::StopWatch stopwatch;
        if (getnameinfo(sa, slen, host, sizeof(host), nullptr, 0, NI_NAMEREQD) == 0)
        {
            if (max_size)
            {
                put_cached_hostname(&socket_address, host, max_size);
            }

            *output = host;
            success = true;
        }
        this_thread.rdns_time += stopwatch.split();
    }

    if (!success)
    {
        *output = ip;
    }
    return success;
}

std::tuple<bool, std::string> reverse_name_lookup(const sockaddr_storage* addr)
{
    bool ok = false;
    std::string res;

    auto addr_fam = addr->ss_family;
    if (addr_fam == AF_INET || addr_fam == AF_INET6)
    {
        socklen_t slen = (addr_fam == AF_INET) ? sizeof(sockaddr_in) : sizeof(sockaddr_in6);
        char host[NI_MAXHOST];
        auto sa = reinterpret_cast<const sockaddr*>(addr);
        auto ret = getnameinfo(sa, slen, host, sizeof(host), nullptr, 0, NI_NAMEREQD);
        if (ret == 0)
        {
            ok = true;
            res = host;
        }
        else
        {
            res = gai_strerror(ret);
        }
    }
    else
    {
        res = mxb::string_printf("Invalid address type %i", addr_fam);
    }

    return {ok, std::move(res)};
}

void reset_name_lookup_timers()
{
    this_thread.dns_time = 0s;
    this_thread.rdns_time = 0s;
}

mxb::Duration name_lookup_duration(NameLookupTimer type)
{
    switch (type)
    {
    case NameLookupTimer::ALL:
        return this_thread.dns_time + this_thread.rdns_time;

    case NameLookupTimer::NORMAL:
        return this_thread.dns_time;

    case NameLookupTimer::REVERSE:
        return this_thread.rdns_time;
    }

    mxb_assert(!true);
    return 0s;
}

std::optional<std::string> get_cached_hostname(const sockaddr_storage* addr)
{
    std::optional<std::string> rval;
    auto it = this_thread.host_cache.find(HostKey(*addr));

    if (it != this_thread.host_cache.end())
    {
        if (mxb::Clock::now() - it->second.last_updated < 300s)
        {
            rval = it->second.hostname;
        }
        else
        {
            this_thread.host_cache.erase(it);
        }
    }

    return rval;
}

void put_cached_hostname(const sockaddr_storage* addr, const std::string& hostname, size_t max_size)
{
    while (this_thread.host_cache.size() > max_size)
    {
        this_thread.host_cache.pop_back();
    }

    if (max_size > 0)
    {
        auto [it, inserted] = this_thread.host_cache.emplace(HostKey {*addr}, HostEntry {hostname});

        if (!inserted)
        {
            // Key already exists, overwrite only the value
            it->second = HostEntry{hostname};
        }
    }
}
}
