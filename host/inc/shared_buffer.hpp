#ifndef KVM_HYPERVISOR_SHARED_BUFFER_HPP
#define KVM_HYPERVISOR_SHARED_BUFFER_HPP

#include "shared_buffer_protocol.h"

#include <array>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <mutex>

struct GuestContext;
struct vm;

struct SharedBufferState {
    std::mutex mutex;
    std::condition_variable condition;
    std::array<std::uint8_t, BUFFER_SIZE> data{};
    std::size_t size = 0;
    std::size_t expectedBytes = 0;
    std::size_t receivedBytes = 0;
    std::size_t readerCount = 0;
    std::size_t completedReaders = 0;
    std::uint64_t generation = 0;
    bool dataReady = false;
    bool roundInProgress = false;
    bool stopped = false;
};

struct SharedBufferVmState {
    std::uint8_t mode = VM_MODE_READER;
    std::size_t readOffset = 0;
    std::uint64_t lastReadGeneration = 0;
    std::uint64_t activeReadGeneration = 0;
    std::uint64_t writeGeneration = 0;
    bool modeDelivered = false;
    bool readCompleted = false;
    bool transferCompleted = false;
};

bool handleSharedBufferIo(GuestContext& context, struct vm& virtualMachine);
void abortSharedBuffer(SharedBufferState& state);

#endif // KVM_HYPERVISOR_SHARED_BUFFER_HPP
