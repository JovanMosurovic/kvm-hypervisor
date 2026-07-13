#include "cli.hpp"
#include "vm_runner.hpp"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <pthread.h>

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

    SharedState sharedState;

    GuestContext guestContext;
    guestContext.id = 0;
    guestContext.memorySize = config.memorySize;
    guestContext.pageSize = config.pageSize;
    guestContext.imagePath = config.guestImage;
    guestContext.sharedState = &sharedState;

    pthread_t thread;

    const int createResult = pthread_create(&thread, nullptr, runGuest, &guestContext);

    if (createResult != 0) {
        std::cerr << "Failed to create VM thread: " << std::strerror(createResult) << '\n';
        return EXIT_FAILURE;
    }

    const int joinResult = pthread_join(thread, nullptr);

    if (joinResult != 0) {
        std::cerr << "Failed to join VM thread: " << std::strerror(joinResult) << '\n';
        return EXIT_FAILURE;
    }

    return guestContext.completedSuccessfully ? EXIT_SUCCESS : EXIT_FAILURE;
}
