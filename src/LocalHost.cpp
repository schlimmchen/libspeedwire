#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#include <cstring>
#include <stdio.h>
#include <chrono>

#ifdef _WIN32
#include <Winsock2.h>
#include <Ws2tcpip.h>
#include <iphlpapi.h>
#else
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <net/if.h>
#endif

#include <LocalHost.hpp>
#include <AddressConversion.hpp>
using namespace libspeedwire;

LocalHost *LocalHost::instance = NULL;


/**
 *  Constructor
 */
LocalHost::LocalHost(void) {}

/**
 *  Destructor
 */
LocalHost::~LocalHost(void) {}

/**
 *  Get singleton instance. The cache of the returned instance cache is fully initialized.
 */
LocalHost& LocalHost::getInstance(void) {
    // check if instance has been instanciated
    if (instance == NULL) {

        // instanciate instance
        instance = new LocalHost();

#ifdef _WIN32
        // initialize Windows Socket API with given VERSION.
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData)) {
            perror("WSAStartup failure");
        }
#endif
        // query hostname
        instance->cacheHostname(LocalHost::queryHostname());

        // query interfaces
        instance->cacheLocalIPAddresses(LocalHost::queryLocalIPAddresses());

        // query interface information, such as interface names, mac addresses and local ip addresses and interface indexes
        const std::vector<LocalHost::InterfaceInfo> infos = LocalHost::queryLocalInterfaceInfos();
        instance->cacheLocalInterfaceInfos(infos);

#if 1 //defined(__arm__) && !defined(__APPLE__) && !defined(_WIN32)
        // on raspi getaddrinfo() is broken, use ip addresses from interface inforamation instead;
        // a broken getaddrinfo() returns the local loopback address 127.0.1.1 or 127.0.0.1 but no other ipv4 address
        bool isBrokenGetAddrInfo = true;
        for (auto& addr : instance->getLocalIPAddresses()) {
            if (AddressConversion::isIpv4(addr) == true && addr.substr(0, 4) != "127.") {
                isBrokenGetAddrInfo = false; break;
            }
        }
        if (isBrokenGetAddrInfo == true) {
            std::vector<std::string> localIpAddress;
            for (auto& info : instance->getLocalInterfaceInfos()) {
                for (auto& addr : info.ip_addresses) {
                    localIpAddress.push_back(addr);
                }
            }
            instance->cacheLocalIPAddresses(localIpAddress);
        }
#endif
    }
    return *instance;
}


/**
 *  Getter for cached hostname
 */
const std::string &LocalHost::getHostname(void) const {
    return hostname;
}

/**
 *  Setter to cache the hostname
 */
void LocalHost::cacheHostname(const std::string &hostname) {
    this->hostname = hostname;
}


/**
 *  Getter for cached ipv4 and ipv6 interface names
 */
const std::vector<std::string>& LocalHost::getLocalIPAddresses(void) const {
    return local_ip_addresses;
}

/**
 *  Getter for cached ipv4 interface names
 */
const std::vector<std::string>& LocalHost::getLocalIPv4Addresses(void) const {
    return local_ipv4_addresses;
}

/**
 *  Getter for cached ipv6 interface names
 */
const std::vector<std::string>& LocalHost::getLocalIPv6Addresses(void) const {
    return local_ipv6_addresses;
}

/**
 *  Setter to cache the interface names
 */
void LocalHost::cacheLocalIPAddresses(const std::vector<std::string> &local_ip_addresses) {
    this->local_ip_addresses = local_ip_addresses;
    local_ipv4_addresses.clear();
    local_ipv6_addresses.clear();
    for (auto& a : local_ip_addresses) {
        std::string addr = AddressConversion::extractIPAddress(a);
        if (AddressConversion::isIpv4(addr) == true) {
            local_ipv4_addresses.push_back(addr);
        } else if (AddressConversion::isIpv6(addr) == true) {
            local_ipv6_addresses.push_back(addr);
        }
    }
}


/**
 *  Getter for cached interface informations
 */
const std::vector<LocalHost::InterfaceInfo> &LocalHost::getLocalInterfaceInfos(void) const {
    return local_interface_infos;
}

/**
 *  Setter to cache the interface informations
 */
void LocalHost::cacheLocalInterfaceInfos(const std::vector<LocalHost::InterfaceInfo> &infos) {
    local_interface_infos = infos;
}

/**
 *  Getter for obtaining the mac address for a given ip address that is associated with a local interface
 */
const std::string LocalHost::getMacAddress(const std::string &local_ip_address) const {
    for (auto &info : local_interface_infos) {
        for (auto &addr : info.ip_addresses) {
            if (local_ip_address == addr) {
                return info.mac_address;
            }
        }
    }
    return "";
}

/**
 *  Getter for obtaining the interface name for a given ip address that is associated with a local interface
 */
const std::string LocalHost::getInterfaceName(const std::string& local_ip_address) const {
    for (auto& info : local_interface_infos) {
        for (auto& addr : info.ip_addresses) {
            if (local_ip_address == addr) {
                return info.if_name;
            }
        }
    }
    return "";
}

/**
 *  Getter for obtaining the interface index for a given ip address that is associated with a local interface.
 *  This is needed for setting up ipv6 multicast sockets.
 */
uint32_t LocalHost::getInterfaceIndex(const std::string & local_ip_address) const {
    for (auto& info : local_interface_infos) {
        for (auto &addr : info.ip_addresses) {
            if (addr == local_ip_address) {
                return info.if_index;
            }
        }
    }
    return (uint32_t)-1;
}

/**
 *  Getter for obtaining the interface address prefix length for a given ip address that is associated with a local interface.
 */
uint32_t LocalHost::getInterfacePrefixLength(const std::string& local_ip_address) const {
    for (auto& info : local_interface_infos) {
        auto it = info.ip_address_prefix_lengths.find(local_ip_address);
        if (it != info.ip_address_prefix_lengths.end()) {
            return it->second;
        }
    }
    return (uint32_t)-1;
}


/**
 *  Query the local hostname from the operating system.
 */
const std::string LocalHost::queryHostname(void) {
    char buffer[256];
    if (gethostname(buffer, sizeof(buffer)) != 0) {
        perror("gethostname() failure");
        return std::string();
    }
    return std::string(buffer);
}


/**
 *  Query all local ipv4 and ipv6 addresses from the operating system.
 */
std::vector<std::string> LocalHost::queryLocalIPAddresses(void) {
    std::vector<std::string> interfaces;
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) != 0) {
        perror("gethostname");
        return interfaces;
    }
    struct addrinfo *info = NULL;
    if (getaddrinfo(hostname, NULL, NULL, &info) != 0) {
        perror("getaddrinfo");
        return interfaces;
    }
    while (info != NULL) {
        if (info->ai_protocol == 0 &&
            (info->ai_family == AF_INET ||
             info->ai_family == AF_INET6)) {
            interfaces.push_back(AddressConversion::toString(*info->ai_addr));
        }
        info = info->ai_next;
    }
    freeaddrinfo(info);
    return interfaces;
}


/**
 *  Query information related to all local interfaces from the operating system.
 */
std::vector<LocalHost::InterfaceInfo> LocalHost::queryLocalInterfaceInfos(void) {
    std::vector<LocalHost::InterfaceInfo> addresses;
#ifdef _WIN32
    PIP_ADAPTER_ADDRESSES AdapterAdresses;
    DWORD dwBufLen = sizeof(PIP_ADAPTER_ADDRESSES);

    AdapterAdresses = (IP_ADAPTER_ADDRESSES *)malloc(sizeof(IP_ADAPTER_ADDRESSES));
    if (AdapterAdresses == NULL) {
        perror("Error allocating memory needed to call GetAdaptersAddresses\n");
        return addresses;
    }

    // Make an initial call to GetAdaptersAddresses to get the necessary size into the dwBufLen variable
    if (GetAdaptersAddresses(AF_UNSPEC, 0, NULL, AdapterAdresses, &dwBufLen) == ERROR_BUFFER_OVERFLOW) {
        free(AdapterAdresses);
        AdapterAdresses = (IP_ADAPTER_ADDRESSES *)malloc(dwBufLen);
        if (AdapterAdresses == NULL) {
            perror("Error allocating memory needed to call GetAdaptersAddresses\n");
            return addresses;
        }
    }

    if (GetAdaptersAddresses(AF_UNSPEC, 0, NULL, AdapterAdresses, &dwBufLen) == NO_ERROR) {
        PIP_ADAPTER_ADDRESSES pAdapterAddresses = AdapterAdresses;
        while (pAdapterAddresses != NULL) {
            if (pAdapterAddresses->OperStatus == IF_OPER_STATUS::IfOperStatusUp && pAdapterAddresses->PhysicalAddressLength == 6) {  // PhysicalAddressLength == 0 for loopback interfaces
                std::array<uint8_t, 6> mac_addr_bytes;
                for (size_t i = 0; i < mac_addr_bytes.size(); ++i) {
                    mac_addr_bytes[i] = pAdapterAddresses->PhysicalAddress[i];
                }
                std::string mac_addr = AddressConversion::toString(mac_addr_bytes);
                LocalHost::InterfaceInfo info;
                char friendly[256];
                snprintf(friendly, sizeof(friendly), "%S", pAdapterAddresses->FriendlyName);
                info.if_name = std::string(friendly);
                info.if_index = pAdapterAddresses->Ipv6IfIndex;
                info.mac_address = mac_addr;

                PIP_ADAPTER_UNICAST_ADDRESS_LH unicast_address = pAdapterAddresses->FirstUnicastAddress;
                do {
                    std::string ip_name = AddressConversion::toString(*(sockaddr*)(unicast_address->Address.lpSockaddr));
                    info.ip_addresses.push_back(ip_name);
                    info.ip_address_prefix_lengths[ip_name] = unicast_address->OnLinkPrefixLength;
                    fprintf(stdout, "address: %28.*s  prefixlength: %d  mac: %s  name: \"%s\"\n", (int)ip_name.length(), ip_name.c_str(), unicast_address->OnLinkPrefixLength, mac_addr.c_str(), info.if_name.c_str());
                    unicast_address = unicast_address->Next;
                } while (unicast_address != NULL);

                addresses.push_back(info);
            }
            pAdapterAddresses = pAdapterAddresses->Next;
        }
    }
    free(AdapterAdresses);
#else
    int s = socket(PF_INET, SOCK_DGRAM, 0);
    struct ifconf ifc;
    struct ifreq* ifr;
    char buf[16384];
    ifc.ifc_len = sizeof(buf);
    ifc.ifc_buf = buf;
    if (ioctl(s, SIOCGIFCONF, &ifc) == 0) {
        ifr = ifc.ifc_req;
        for (int i = 0; i < ifc.ifc_len;) {
            struct ifreq buffer;
            memset(&buffer, 0x00, sizeof(buffer));
            strcpy(buffer.ifr_name, ifr->ifr_name);
            InterfaceInfo info;
            info.if_name = std::string(buffer.ifr_name);
            info.if_index = if_nametoindex(buffer.ifr_name);
            std::string ip_name = AddressConversion::toString(ifr->ifr_ifru.ifru_addr);
            info.ip_addresses.push_back(ip_name);

#ifndef __APPLE__
            if (ioctl(s, SIOCGIFHWADDR, &buffer) == 0) {
                struct sockaddr saddr = buffer.ifr_ifru.ifru_hwaddr;
                std::array<uint8_t, 6> mac_addr_bytes;
                for (size_t i = 0; i < mac_addr_bytes.size(); ++i) {
                    mac_addr_bytes[i] = (uint8_t)saddr.sa_data[i];
                }
                std::string mac_addr = AddressConversion::toString(mac_addr_bytes);
                info.mac_address = mac_addr;
            }
            size_t len = sizeof(struct ifreq);

            /* try to get network mask */
            uint32_t prefix = 0;
            if (ioctl(s, SIOCGIFNETMASK, &buffer) == 0) {
                struct sockaddr smask = buffer.ifr_ifru.ifru_netmask;
                if (smask.sa_family == AF_INET) {
                    struct sockaddr_in& smaskv4 = AddressConversion::toSockAddrIn(smask);
                    uint32_t saddr = smaskv4.sin_addr.s_addr;
                    for (int i = 0; i < 32; ++i) {
                        if ((saddr & (1u << i)) == 0) break;
                        ++prefix;
                    }
                }
                else if (smask.sa_family == AF_INET6) {
                    struct sockaddr_in6 smaskv6 = AddressConversion::toSockAddrIn6(smask);
                    for (int i = 0; i < sizeof(smaskv6.sin6_addr.s6_addr); ++i) {
                        uint8_t b = smaskv6.sin6_addr.s6_addr[i];
                        for (int j = 0; j < 8; ++j) {
                            if ((b & (1u << j)) == 0) break;
                            ++prefix;
                        }
                    }
                }
            }
            info.ip_address_prefix_lengths[ip_name] = prefix;
            fprintf(stdout, "address: %28.*s  prefixlength: %d  mac: %s  name: \"%s\"\n", (int)ip_name.length(), ip_name.c_str(), prefix, info.mac_address.c_str(), info.if_name.c_str());
#else
            size_t len = IFNAMSIZ + ifr->ifr_addr.sa_len;
#endif
            addresses.push_back(info);

            ifr = (struct ifreq*)((char*)ifr + len);
            i += len;
        }
    }
    close(s);
#endif
    return addresses;
}



/**
 *  Platform neutral sleep method.
 */
void LocalHost::sleep(uint32_t millis) {
#ifdef _WIN32
    Sleep(millis);
#else
    ::sleep(millis / 1000);
#endif
}


/**
 *  Platform neutral method to get a tick count provided in ms ticks; this is useful for timing purposes.
 */
uint64_t LocalHost::getTickCountInMs(void) {  // return a tick counter with ms resolution
#if 1
    std::chrono::steady_clock::duration time = std::chrono::steady_clock::now().time_since_epoch();  // this is not relative to the unix epoch(!)
    std::chrono::milliseconds time_in_ms = std::chrono::duration_cast<std::chrono::milliseconds>(time);
    return time_in_ms.count();
#else
    // fallback code, in case std::chrono cannot be used
#ifdef _WIN32
    return GetTickCount64();
#else
    struct timespec spec;
    if (clock_gettime(CLOCK_MONOTONIC, &spec) == -1) {
        perror("clock_gettime(CLOCK_MONOTONIC,) failure");
    }
    return spec.tv_sec * 1000 + spec.tv_nsec / 1000000;
#endif
#endif
}


/**
 *  Platform neutral method to get the unix epoch time in ms.
 */
uint64_t LocalHost::getUnixEpochTimeInMs(void) {
#if 1
    std::chrono::system_clock::duration time = std::chrono::system_clock::now().time_since_epoch();
    std::chrono::milliseconds time_in_ms = std::chrono::duration_cast<std::chrono::milliseconds>(time);
    return time_in_ms.count();
#else
    // fallback code, in case std::chrono cannot be used
    struct timespec spec;
    if (clock_gettime(CLOCK_REALTIME, &spec) == -1) {
        perror("clock_gettime(CLOCK_REALTIME,) failure");
    }
    return spec.tv_sec * 1000 + spec.tv_nsec / 1000000;
#endif
}


/**
 *  Platform neutral conversion of unix epoch time in ms to a formatted string.
 */
std::string LocalHost::unixEpochTimeInMsToString(uint64_t epoch) {
    std::string result;
    const time_t time = epoch / 1000u;
    struct tm* ts_ptr = localtime(&time);
    if (ts_ptr != NULL) {
        char buf[80];
        strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", ts_ptr);
        result.append(buf);
    }
    return result;
}


/**
 *  Calculate the absolute time difference between time1 and time2
 */
uint64_t LocalHost::calculateAbsTimeDifference(uint64_t time1, uint64_t time2) {
    int64_t signed_diff = (int64_t)(time1 - time2);
    uint64_t abs_diff = (signed_diff >= 0 ? signed_diff : -signed_diff);
    return abs_diff;
}


/**
 *  Match given ip address to local interface ip address that resides on the same subnet.
 */
const std::string LocalHost::getMatchingLocalIPAddress(std::string ip_address) const {
    if (AddressConversion::isIpv4(ip_address) == true) {
        // try to find an interface, where the subnet covers the given ip address
        struct in_addr in_ip_address = AddressConversion::toInAddress(ip_address);
        for (auto& if_addr : local_ipv4_addresses) {
            struct in_addr in_if_addr = AddressConversion::toInAddress(if_addr);
            uint32_t if_prefix_length = LocalHost::getInterfacePrefixLength(if_addr);
            if (AddressConversion::resideOnSameSubnet(in_if_addr, in_ip_address, if_prefix_length) == true) {
                return if_addr;
            }
        }
#if defined(_WIN32)
        DWORD dBestIfIndex;
        DWORD result = GetBestInterface(in_ip_address.s_addr, &dBestIfIndex);
        if (result == 0) {
            const std::vector<InterfaceInfo>& local_interfaces = getLocalInterfaceInfos();
            for (const auto& local_interf : local_interfaces) {
                if (local_interf.if_index == dBestIfIndex) {
                    for (const auto& if_addr : local_interf.ip_addresses) {
                        if (AddressConversion::isIpv4(if_addr)) {
                            return if_addr;
                        }
                    }
                }
            }
        }
#endif
        // if there is only one interface, just use it
        if (local_ipv4_addresses.size() == 1) {
            return local_ipv4_addresses[0];
        }
    }
    else if (AddressConversion::isIpv6(ip_address) == true) {
        struct in6_addr in_ip_address = AddressConversion::toIn6Address(ip_address);
        for (auto& if_addr : local_ipv6_addresses) {
            struct in6_addr in_if_addr = AddressConversion::toIn6Address(if_addr);
            uint32_t if_prefix_length  = LocalHost::getInterfacePrefixLength(if_addr);
            if (AddressConversion::resideOnSameSubnet(in_if_addr, in_ip_address, if_prefix_length) == true) {
                return if_addr;
            }
        }
        if (local_ipv6_addresses.size() == 1) {
            return local_ipv6_addresses[0];
        }
    }
    return "";
}


/**
 *  Print buffer content to stdout
 */
void LocalHost::hexdump(const void* const buff, const unsigned long size) {
    printf("--------:");
    for (unsigned long i = 0; i < size; i++) {
        if ((i % 16) == 0) {
            if (i != 0) {
                printf("     ");
                for (unsigned long j = (i >= 16 ? i - 16 : 0); j < i; j++) {
                    const unsigned char c = ((unsigned char*)buff)[j];
                    putchar(isprint(c) ? c : 0x001a);
                }
            }
            printf("\n%08X: ", i);
        }
        printf("%02X ", ((unsigned char*)buff)[i]);
    }
    printf("\n");
    fflush(stdout);
}
