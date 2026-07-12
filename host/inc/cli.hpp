#ifndef KVM_HYPERVISOR_CLI_HPP
#define KVM_HYPERVISOR_CLI_HPP

#include <cstddef>
#include <string>

enum class CliStatus {
    Ok,
    Help,
    Error
};

struct Config {
    std::size_t memorySize = 0;
    std::string guestImage;
};

struct CliResult {
    CliStatus status = CliStatus::Error;
    Config config;
};

CliResult parseArguments(int argc, char *argv[]);

#endif //KVM_HYPERVISOR_CLI_HPP
