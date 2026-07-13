#ifndef KVM_HYPERVISOR_VM_RUNNER_HPP
#define KVM_HYPERVISOR_VM_RUNNER_HPP

#include <cstddef>
#include <mutex>
#include <string>

struct SharedState {
    std::mutex consoleMutex;
    std::mutex serialInputMutex;
};

struct GuestContext {
    std::size_t id = 0;
    std::size_t memorySize = 0;
    std::size_t pageSize = 0;
    std::string imagePath;
    std::string serialOutputBuffer;
    SharedState *sharedState = nullptr;
    bool completedSuccessfully = false;
};

void *runGuest(void *argument);

#endif //KVM_HYPERVISOR_VM_RUNNER_HPP
