#include "cli.hpp"
#include "file_io.hpp"
#include "shared_buffer.hpp"
#include "vm_runner.hpp"

#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <pthread.h>
#include <string>
#include <system_error>
#include <vector>

namespace {

    // File system 8
    bool configureSharedFiles(const Config& config, SharedState& sharedState)
    {
        for (const std::string& configuredPath : config.sharedFiles) {
            std::error_code error;
            const std::filesystem::path path = std::filesystem::canonical(configuredPath, error); //convert file path to absolute path

            if (error || !std::filesystem::is_regular_file(path, error)) {
                std::cerr << "Error: shared file '" << configuredPath << "' does not exist or is not a regular file\n";
                return false;
            }

            const std::string name = std::filesystem::path(configuredPath).filename().string();

            if (!isValidGuestFileName(name)) {
                std::cerr << "Error: shared file name '" << name << "' is not valid for guest access\n";
                return false;
            }

            if (!sharedState.sharedFiles.emplace(name, path.string()).second) {     // pair "guest.txt" -> "/tmp/aor2/guest.txt"
                std::cerr << "Error: multiple shared files use the guest name '" << name << "'\n";
                return false;
            }
        }

        return true;
    }

} // namespace

int main(int argc, char *argv[])
{
    const CliResult cli = parseArguments(argc, argv);

    if (cli.status == CliStatus::Help) {
        return EXIT_SUCCESS;
    }

    if (cli.status == CliStatus::Error) {
        return EXIT_FAILURE;
    }

    const Config& config = cli.config;
    const std::size_t guestCount = config.guestImages.size();

    SharedState sharedState;
    sharedState.sharedBuffer.readerCount = guestCount - 1;

    if (!configureSharedFiles(config, sharedState)) {
        return EXIT_FAILURE;
    }

    std::vector<GuestContext> guestContexts(guestCount);
    std::vector<pthread_t> threads(guestCount);

    for (std::size_t i = 0; i < guestCount; ++i) {
        guestContexts[i].id = i;
        guestContexts[i].memorySize = config.memorySize;
        guestContexts[i].pageSize = config.pageSize;
        guestContexts[i].imagePath = config.guestImages[i];
        // Interrupt 2
        guestContexts[i].sharedBuffer.mode = i == 0 ? VM_MODE_WRITER : VM_MODE_READER;
        guestContexts[i].sharedState = &sharedState; // Every VM gets separate GuestContext, but all of them point to the same sharedState
    }

    std::size_t startedThreads = 0;
    bool allGuestsSucceeded = true;

    for (std::size_t i = 0; i < guestCount; ++i) {
        const int result = pthread_create(&threads[i], nullptr, runGuest, &guestContexts[i]);

        if (result != 0) {
            std::cerr << "Failed to create VM thread: " << std::strerror(result) << '\n';
            allGuestsSucceeded = false;
            abortSharedBuffer(sharedState.sharedBuffer);
            break;
        }

        ++startedThreads;
    }

    for (std::size_t i = 0; i < startedThreads; ++i) {
        const int result = pthread_join(threads[i], nullptr);

        if (result != 0) {
            std::cerr << "Failed to join VM thread: " << std::strerror(result) << '\n';
            allGuestsSucceeded = false;
            abortSharedBuffer(sharedState.sharedBuffer);
            continue;
        }

        if (!guestContexts[i].completedSuccessfully) {
            allGuestsSucceeded = false;
        }
    }

    return allGuestsSucceeded ? EXIT_SUCCESS : EXIT_FAILURE;
}
