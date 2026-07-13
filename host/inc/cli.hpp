#ifndef KVM_HYPERVISOR_CLI_HPP
#define KVM_HYPERVISOR_CLI_HPP

#include <cstddef>
#include <string>
#include <vector>

enum class CliStatus {
    Ok,
    Help,
    Error
};

struct Config {
    std::size_t memorySize = 0;
    std::size_t pageSize = 0;
    std::vector<std::string> guestImages;
};

struct CliResult {
    CliStatus status = CliStatus::Error;
    Config config;
};

CliResult parseArguments(int argc, char *argv[]);

#endif //KVM_HYPERVISOR_CLI_HPP
