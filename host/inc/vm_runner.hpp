#ifndef KVM_HYPERVISOR_VM_RUNNER_HPP
#define KVM_HYPERVISOR_VM_RUNNER_HPP

#include "file_io.hpp"

#include <cstddef>
#include <mutex>
#include <string>
#include <unordered_map>

struct SharedState {
    std::mutex consoleMutex;
    std::mutex serialInputMutex;
    std::unordered_map<std::string, std::string> sharedFiles;
};

struct GuestContext {
    std::size_t id = 0;
    std::size_t memorySize = 0;
    std::size_t pageSize = 0;
    std::string imagePath;
    std::string serialOutputBuffer;
    FileState fileState;
    SharedState *sharedState = nullptr;
    bool completedSuccessfully = false;
};

void *runGuest(void *argument);

#endif //KVM_HYPERVISOR_VM_RUNNER_HPP
