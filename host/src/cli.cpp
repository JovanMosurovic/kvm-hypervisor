#include "cli.hpp"

#include <iostream>
#include <string>
#include <string_view>

namespace {

    constexpr std::size_t MiB = 1024u * 1024u;

    void printGeneralUsage(std::ostream& output, std::string_view program)
    {
        output
            << "Usage:\n"
            << "  " << program << " -m <2|4|8> <guest-image>\n\n"
            << "Options:\n"
            << "  -m, --memory <2|4|8>   Guest memory size in MiB\n"
            << "  -h, --help             Show this help\n";
    }

    void printMemoryError(
        std::string_view program,
        std::string_view message)
    {
        std::cerr
            << "Error: " << message << "\n\n"
            << "Memory option usage:\n"
            << "  " << program << " -m <2|4|8> <guest-image>\n"
            << "  " << program << " --memory <2|4|8> <guest-image>\n\n"
            << "Example:\n"
            << "  " << program << " -m 4 guest.img\n";
    }

    bool parseMemoryValue(
        std::string_view value,
        std::size_t& memorySize)
    {
        if (value == "2") {
            memorySize = 2u * MiB;
            return true;
        }

        if (value == "4") {
            memorySize = 4u * MiB;
            return true;
        }

        if (value == "8") {
            memorySize = 8u * MiB;
            return true;
        }

        return false;
    }

} // namespace

CliResult parseArguments(int argc, char *argv[])
{
    CliResult result;
    bool memorySet = false;

    for (int i = 1; i < argc; ++i) {
        std::string_view argument = argv[i];

        if (argument == "-h" || argument == "--help") {
            printGeneralUsage(std::cout, argv[0]);
            result.status = CliStatus::Help;
            return result;
        }

        if (argument == "-m" || argument == "--memory") {
            if (memorySet) {
                printMemoryError(
                    argv[0],
                    "memory option was specified more than once");
                return result;
            }

            if (i + 1 >= argc) {
                printMemoryError(
                    argv[0],
                    "memory option requires a value");
                return result;
            }

            std::string_view value = argv[++i];

            if (!parseMemoryValue(value, result.config.memorySize)) {
                printMemoryError(
                    argv[0],
                    "memory size must be 2, 4 or 8 MiB");
                return result;
            }

            memorySet = true;
            continue;
        }

        if (!argument.empty() && argument.front() == '-') {
            std::cerr << "Error: unknown option '" << argument << "'\n\n";
            printGeneralUsage(std::cerr, argv[0]);
            return result;
        }

        if (!result.config.guestImage.empty()) {
            std::cerr << "Error: more than one guest image was provided\n\n";
            printGeneralUsage(std::cerr, argv[0]);
            return result;
        }

        result.config.guestImage = argument;
    }

    if (!memorySet) {
        printMemoryError(argv[0], "memory option is required");
        return result;
    }

    if (result.config.guestImage.empty()) {
        std::cerr << "Error: guest image is required\n\n";
        printGeneralUsage(std::cerr, argv[0]);
        return result;
    }

    result.status = CliStatus::Ok;
    return result;
}
