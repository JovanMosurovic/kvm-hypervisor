#include "vm_runner.hpp"

#include "file_io.hpp"
#include "file_protocol.h"
#include "shared_buffer.hpp"
#include "shared_buffer_protocol.h"

extern "C" {
#include "vm.h"
}

#include <cerrno>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <linux/kvm.h>
#include <mutex>
#include <string_view>
#include <sys/ioctl.h>

namespace {

    void printError(GuestContext& context, std::string_view message)
    {
        std::lock_guard<std::mutex> lock(context.sharedState->consoleMutex);

        std::cerr
            << "[VM " << context.id << "] "
            << message << '\n';
    }

    void printSystemError(GuestContext& context, std::string_view operation, int errorNumber)
    {
        std::lock_guard<std::mutex> lock(context.sharedState->consoleMutex);

        std::cerr
            << "[VM " << context.id << "] "
            << operation << " failed: "
            << std::strerror(errorNumber)
            << " (errno " << errorNumber << ")\n";
    }

    void printUnexpectedExit(GuestContext& context, unsigned int exitReason)
    {
        std::lock_guard<std::mutex> lock(context.sharedState->consoleMutex);

        std::cout
            << "[VM " << context.id << "] "
            << "Unexpected VM exit. Exit code: "
            << exitReason << '\n';
    }

    void printHalt(GuestContext& context)
    {
        std::lock_guard<std::mutex> lock(context.sharedState->consoleMutex);

        std::cout
            << "[VM " << context.id << "] "
            << "KVM_EXIT_HLT\n";
    }

    void flushSerialOutput(GuestContext& context, bool finishLine)
    {
        if (context.serialOutputBuffer.empty()) {
            return;
        }

        std::lock_guard<std::mutex> lock(context.sharedState->consoleMutex);

        std::cout << "[VM " << context.id << "] " << context.serialOutputBuffer;

        if (finishLine && context.serialOutputBuffer.back() != '\n') {
            std::cout << '\n';
        }

        std::cout << std::flush;
        context.serialOutputBuffer.clear();
    }

    bool handleSerialIo(GuestContext& context, struct vm& virtualMachine)
    {
        const auto& io = virtualMachine.run->io;

        if (io.port != 0xE9 || io.size != 1 || io.count != 1) {

            std::lock_guard<std::mutex> lock(context.sharedState->consoleMutex);

            std::cerr
                << "[VM " << context.id << "] "
                << "Unexpected I/O exit. Exit code: "
                << virtualMachine.run->exit_reason
                << ", port: 0x"
                << std::hex << io.port << std::dec
                << ", size: " << static_cast<unsigned int>(io.size)
                << ", count: " << io.count
                << '\n';

            return false;
        }

        const std::size_t dataOffset = static_cast<std::size_t>(io.data_offset);

        const std::size_t mappingSize = static_cast<std::size_t>(virtualMachine.run_mmap_size);

        if (dataOffset >= mappingSize) {
            printError(context, "I/O data offset is outside the KVM run mapping");
            return false;
        }

        auto *data = reinterpret_cast<std::uint8_t *>(virtualMachine.run) + dataOffset;

        if (io.direction == KVM_EXIT_IO_OUT) {
            const char output = static_cast<char>(*data);

            context.serialOutputBuffer.push_back(output);

            if (output == '\n') {
                flushSerialOutput(context, false);
            }

            return true;
        }

        if (io.direction == KVM_EXIT_IO_IN) {
            char input;

            flushSerialOutput(context, true);

            std::lock_guard<std::mutex> lock(context.sharedState->serialInputMutex);

            if (!std::cin.get(input)) {
                printError(context, "Failed to read serial input");
                return false;
            }

            *data = static_cast<std::uint8_t>(input);
            return true;
        }

        printError(context, "Unexpected I/O direction");
        return false;
    }

    bool runVirtualMachine(GuestContext& context)
    {
        struct vm virtualMachine;
        /* sregs holds control and segment registers; regs holds regular CPU registers. */
        struct kvm_sregs specialRegisters{};
        struct kvm_regs registers{};

        int interruptCount = IRQ_COUNT;

        if (vm_init(&virtualMachine, context.memorySize) != 0) {

            printError(context, "Failed to initialize the virtual machine");

            vm_destroy(&virtualMachine);
            return false;
        }

        if (ioctl(virtualMachine.vcpu_fd, KVM_GET_SREGS, &specialRegisters) < 0) {

            const int errorNumber = errno;

            printSystemError(context, "KVM_GET_SREGS", errorNumber);

            vm_destroy(&virtualMachine);
            return false;
        }

        setup_long_mode(&virtualMachine, &specialRegisters, context.pageSize);

        if (ioctl(virtualMachine.vcpu_fd, KVM_SET_SREGS, &specialRegisters) < 0) {

            const int errorNumber = errno;

            printSystemError(context, "KVM_SET_SREGS", errorNumber);

            vm_destroy(&virtualMachine);
            return false;
        }

        if (load_guest_image(&virtualMachine, context.imagePath.c_str(), GUEST_START_ADDR) < 0) {

            printError(context, "Failed to load guest image");

            vm_destroy(&virtualMachine);
            return false;
        }

        registers.rflags = 0x2;                  /* Reserved bit 1 must be set */
        registers.rip = GUEST_START_ADDR;        /* First guest instruction */
        registers.rsp = virtualMachine.mem_size; /* Stack starts at the top of guest RAM */

        if (ioctl(virtualMachine.vcpu_fd, KVM_SET_REGS, &registers) < 0) {

            const int errorNumber = errno;

            printSystemError(context, "KVM_SET_REGS", errorNumber);

            vm_destroy(&virtualMachine);
            return false;
        }

        /* Ask KVM to return when the guest is ready to accept an interrupt. */
        virtualMachine.run->request_interrupt_window = interruptCount > 0;

        while (true) {
            /* Guest execution continues until KVM needs the host to handle an event. */
            const int result = ioctl(virtualMachine.vcpu_fd, KVM_RUN, 0);

            if (result < 0) {
                const int errorNumber = errno;

                if (errorNumber == EINTR) {
                    continue;
                }

                printSystemError(context, "KVM_RUN", errorNumber);

                vm_destroy(&virtualMachine);
                return false;
            }

            switch (virtualMachine.run->exit_reason) {
            case KVM_EXIT_IO: {
                const std::uint16_t port = virtualMachine.run->io.port;
                bool handled;

                if (port == FILE_IO_PORT) {
                    handled = handleFileIo(context, virtualMachine);
                } else if (port == SHARED_BUFFER_PORT || port == SHARED_BUFFER_STATUS_PORT) {
                    handled = handleSharedBufferIo(context, virtualMachine);
                } else {
                    handled = handleSerialIo(context, virtualMachine);
                }

                if (!handled) {
                    flushSerialOutput(context, true);
                    printError(context, "Failed to handle an I/O exit");
                    vm_destroy(&virtualMachine);
                    return false;
                }
                break;
            }

            case KVM_EXIT_IRQ_WINDOW_OPEN:
                if (interruptCount > 0) {
                    if (inject_irq(&virtualMachine, IRQ_NUM) < 0) {

                        printError(context, "Failed to inject an interrupt");

                        vm_destroy(&virtualMachine);
                        return false;
                    }

                    --interruptCount;
                }

                virtualMachine.run->request_interrupt_window = interruptCount > 0;

                break;

            case KVM_EXIT_HLT:
                flushSerialOutput(context, true);

                if (!context.sharedBuffer.transferCompleted) {
                    printError(context, "VM halted before completing the shared buffer transfer");
                    vm_destroy(&virtualMachine);
                    return false;
                }

                printHalt(context);
                vm_destroy(&virtualMachine);
                return true;

            case KVM_EXIT_SHUTDOWN:
                flushSerialOutput(context, true);
                printUnexpectedExit(context, virtualMachine.run->exit_reason);

                vm_destroy(&virtualMachine);
                return false;

            default:
                flushSerialOutput(context, true);
                printUnexpectedExit(context, virtualMachine.run->exit_reason);

                vm_destroy(&virtualMachine);
                return false;
            }
        }
    }

} // namespace

void *runGuest(void *argument)
{
    auto *context = static_cast<GuestContext *>(argument);

    if (context == nullptr || context->sharedState == nullptr) {
        return nullptr;
    }

    context->completedSuccessfully = runVirtualMachine(*context);

    if (!context->completedSuccessfully) {
        abortSharedBuffer(context->sharedState->sharedBuffer);
    }

    closeGuestFiles(*context);

    return nullptr;
}
