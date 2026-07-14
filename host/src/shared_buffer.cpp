#include "shared_buffer.hpp"

#include "vm.h"
#include "vm_runner.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <linux/kvm.h>
#include <mutex>

namespace {

    bool getIoData(struct vm& virtualMachine, std::uint8_t *&data)
    {
        const auto& io = virtualMachine.run->io;

        if (io.count != 1 || (io.size != 1 && io.size != sizeof(std::uint32_t))) {
            return false;
        }

        const std::size_t dataOffset = static_cast<std::size_t>(io.data_offset);
        const std::size_t mappingSize = static_cast<std::size_t>(virtualMachine.run_mmap_size);
        const std::size_t dataSize = static_cast<std::size_t>(io.size);

        if (dataOffset > mappingSize || dataSize > mappingSize - dataOffset) {
            return false;
        }

        data = reinterpret_cast<std::uint8_t *>(virtualMachine.run) + dataOffset;
        return true;
    }

    std::uint32_t readUint32(const std::uint8_t *data)
    {
        std::uint32_t value;
        std::memcpy(&value, data, sizeof(value));
        return value;
    }

    void writeUint32(std::uint8_t *data, std::uint32_t value)
    {
        std::memcpy(data, &value, sizeof(value));
    }

    void finishWriterData(GuestContext& context, SharedBufferState& state)
    {
        state.dataReady = true;
        ++state.generation;
        context.sharedBuffer.writeGeneration = state.generation;
        state.condition.notify_all();
    }

    bool deliverMode(GuestContext& context, struct vm& virtualMachine, std::uint8_t *data)
    {
        const auto& io = virtualMachine.run->io;

        if (io.port != SHARED_BUFFER_PORT || io.direction != KVM_EXIT_IO_IN || io.size != 1) {
            return false;
        }

        *data = context.sharedBuffer.mode;
        context.sharedBuffer.modeDelivered = true;
        return true;
    }

    bool beginWriterRound(GuestContext& context, const std::uint8_t *data)
    {
        SharedBufferState& state = context.sharedState->sharedBuffer;
        const std::uint32_t requestedBytes = readUint32(data);
        std::unique_lock<std::mutex> lock(state.mutex);

        state.condition.wait(lock, [&state] {
            return state.stopped || !state.roundInProgress;
        });

        if (state.stopped) {
            return false;
        }

        state.expectedBytes = requestedBytes;
        state.receivedBytes = 0;
        state.size = std::min<std::size_t>(requestedBytes, BUFFER_SIZE);
        state.completedReaders = 0;
        state.dataReady = false;
        state.roundInProgress = true;
        context.sharedBuffer.writeGeneration = 0;

        if (requestedBytes == 0) {
            finishWriterData(context, state);
        }

        return true;
    }

    bool writeSharedByte(GuestContext& context, std::uint8_t value)
    {
        SharedBufferState& state = context.sharedState->sharedBuffer;
        std::lock_guard<std::mutex> lock(state.mutex);

        if (state.stopped || !state.roundInProgress || state.dataReady ||
            state.receivedBytes >= state.expectedBytes) {
            return false;
        }

        /* Consume every byte sent by the guest, but store only what fits in the buffer. */
        if (state.receivedBytes < BUFFER_SIZE) {
            state.data[state.receivedBytes] = value;
        }

        ++state.receivedBytes;

        if (state.receivedBytes == state.expectedBytes) {
            finishWriterData(context, state);
        }

        return true;
    }

    bool finishWriterRound(GuestContext& context, std::uint8_t *data)
    {
        SharedBufferState& state = context.sharedState->sharedBuffer;
        const std::uint64_t generation = context.sharedBuffer.writeGeneration;
        std::unique_lock<std::mutex> lock(state.mutex);

        if (generation == 0) {
            return false;
        }

        /* Do not return the final count until all readers acknowledge the data. */
        state.condition.wait(lock, [&state, generation] {
            return state.stopped ||
                (state.dataReady && state.generation == generation &&
                 state.completedReaders == state.readerCount);
        });

        if (state.stopped) {
            return false;
        }

        writeUint32(data, static_cast<std::uint32_t>(state.size));
        state.roundInProgress = false;
        context.sharedBuffer.writeGeneration = 0;
        context.sharedBuffer.transferCompleted = true;
        state.condition.notify_all();
        return true;
    }

    bool beginReaderRound(GuestContext& context, std::uint8_t *data)
    {
        SharedBufferState& state = context.sharedState->sharedBuffer;
        std::unique_lock<std::mutex> lock(state.mutex);

        if (context.sharedBuffer.activeReadGeneration > context.sharedBuffer.lastReadGeneration &&
            !context.sharedBuffer.readCompleted) {
            return false;
        }

        state.condition.wait(lock, [&context, &state] {
            return state.stopped ||
                (state.dataReady && state.generation > context.sharedBuffer.lastReadGeneration);
        });

        if (state.stopped) {
            return false;
        }

        context.sharedBuffer.readOffset = 0;
        context.sharedBuffer.activeReadGeneration = state.generation;
        context.sharedBuffer.readCompleted = false;
        writeUint32(data, static_cast<std::uint32_t>(state.size));
        return true;
    }

    bool readSharedByte(GuestContext& context, std::uint8_t *data)
    {
        SharedBufferState& state = context.sharedState->sharedBuffer;
        std::lock_guard<std::mutex> lock(state.mutex);

        if (state.stopped || context.sharedBuffer.activeReadGeneration != state.generation ||
            context.sharedBuffer.readOffset >= state.size) {
            return false;
        }

        *data = state.data[context.sharedBuffer.readOffset++];
        return true;
    }

    bool finishReaderRound(GuestContext& context, const std::uint8_t *data)
    {
        SharedBufferState& state = context.sharedState->sharedBuffer;
        const std::uint32_t reportedBytes = readUint32(data);
        std::lock_guard<std::mutex> lock(state.mutex);

        if (state.stopped || context.sharedBuffer.readCompleted ||
            context.sharedBuffer.activeReadGeneration != state.generation ||
            context.sharedBuffer.readOffset != state.size || reportedBytes != state.size ||
            state.completedReaders >= state.readerCount) {
            return false;
        }

        context.sharedBuffer.readCompleted = true;
        context.sharedBuffer.lastReadGeneration = context.sharedBuffer.activeReadGeneration;
        context.sharedBuffer.transferCompleted = true;
        ++state.completedReaders;
        state.condition.notify_all();
        return true;
    }

} // namespace

// Interrupt 10
bool handleSharedBufferIo(GuestContext& context, struct vm& virtualMachine)
{
    const auto& io = virtualMachine.run->io;
    std::uint8_t *data = nullptr;

    if ((io.port != SHARED_BUFFER_PORT && io.port != SHARED_BUFFER_STATUS_PORT) ||
        !getIoData(virtualMachine, data)) {
        return false;
    }

    if (!context.sharedBuffer.modeDelivered) {
        return deliverMode(context, virtualMachine, data);
    }

    if (io.port == SHARED_BUFFER_PORT && context.sharedBuffer.mode == VM_MODE_WRITER) {
        if (io.direction == KVM_EXIT_IO_OUT && io.size == sizeof(std::uint32_t)) {
            return beginWriterRound(context, data);
        }

        if (io.direction == KVM_EXIT_IO_OUT && io.size == 1) {
            return writeSharedByte(context, *data);
        }

        return false;
    }

    if (io.port == SHARED_BUFFER_PORT && context.sharedBuffer.mode == VM_MODE_READER) {
        if (io.direction == KVM_EXIT_IO_IN && io.size == sizeof(std::uint32_t)) {
            return beginReaderRound(context, data);
        }

        if (io.direction == KVM_EXIT_IO_IN && io.size == 1) {
            return readSharedByte(context, data);
        }

        return false;
    }

    if (io.port == SHARED_BUFFER_STATUS_PORT && context.sharedBuffer.mode == VM_MODE_WRITER &&
        io.direction == KVM_EXIT_IO_IN && io.size == sizeof(std::uint32_t)) {
        return finishWriterRound(context, data);
    }

    if (io.port == SHARED_BUFFER_STATUS_PORT && context.sharedBuffer.mode == VM_MODE_READER &&
        io.direction == KVM_EXIT_IO_OUT && io.size == sizeof(std::uint32_t)) {
        return finishReaderRound(context, data);
    }

    return false;
}

void abortSharedBuffer(SharedBufferState& state)
{
    std::lock_guard<std::mutex> lock(state.mutex);
    state.stopped = true;
    state.condition.notify_all();
}
