#include "cli.hpp"

extern "C" {
#include "vm.h"
}

#include <cstdlib>
#include <iostream>
#include <linux/kvm.h>
#include <sys/ioctl.h>

int main(int argc, char *argv[])
{
    CliResult cli = parseArguments(argc, argv);

    if (cli.status == CliStatus::Help) {
        return EXIT_SUCCESS;
    }

    if (cli.status == CliStatus::Error) {
        return EXIT_FAILURE;
    }

    const Config& config = cli.config;

    struct vm virtualMachine;
    struct kvm_sregs specialRegisters;
    struct kvm_regs registers{};

    bool stop = false;
    int interruptCount = IRQ_COUNT;

    /*
     * config.memorySize contains 2, 4, or 8 MiB, already converted to bytes
     */
    if (vm_init(&virtualMachine, config.memorySize) != 0) {
        std::cerr << "Failed to initialize the virtual machine.\n";
        return EXIT_FAILURE;
    }

    if (ioctl(
            virtualMachine.vcpu_fd,
            KVM_GET_SREGS,
            &specialRegisters) < 0) {
        std::cerr << "KVM_GET_SREGS failed.\n";
        vm_destroy(&virtualMachine);
        return EXIT_FAILURE;
    }

    setup_long_mode(&virtualMachine, &specialRegisters, config.pageSize);

    if (ioctl(
            virtualMachine.vcpu_fd,
            KVM_SET_SREGS,
            &specialRegisters) < 0) {
        std::cerr << "KVM_SET_SREGS failed.\n";
        vm_destroy(&virtualMachine);
        return EXIT_FAILURE;
    }

    if (load_guest_image(
            &virtualMachine,
            config.guestImage.c_str(),
            GUEST_START_ADDR) < 0) {
        std::cerr << "Failed to load guest image.\n";
        vm_destroy(&virtualMachine);
        return EXIT_FAILURE;
    }

    /*
     * Guest code is linked and loaded at GUEST_START_ADDR.
     * The stack starts at the top of guest memory.
     */
    registers.rflags = 0x2;
    registers.rip = GUEST_START_ADDR;
    registers.rsp = virtualMachine.mem_size;

    if (ioctl(
            virtualMachine.vcpu_fd,
            KVM_SET_REGS,
            &registers) < 0) {
        std::cerr << "KVM_SET_REGS failed.\n";
        vm_destroy(&virtualMachine);
        return EXIT_FAILURE;
    }

    /*
     * Demonstrate interrupt injection
     */
    virtualMachine.run->request_interrupt_window =
        interruptCount > 0;

    while (!stop) {
        int result = ioctl(
            virtualMachine.vcpu_fd,
            KVM_RUN,
            0);

        if (result < 0) {
            std::cerr << "KVM_RUN failed.\n";
            vm_destroy(&virtualMachine);
            return EXIT_FAILURE;
        }

        switch (virtualMachine.run->exit_reason) {
        case KVM_EXIT_IO:
            if (virtualMachine.run->io.direction == KVM_EXIT_IO_OUT &&
                virtualMachine.run->io.port == 0xE9) {

                auto *data =
                    reinterpret_cast<char *>(virtualMachine.run) +
                    virtualMachine.run->io.data_offset;

                std::cout << *data << std::flush;
            }
            break;

        case KVM_EXIT_IRQ_WINDOW_OPEN:
            if (interruptCount > 0) {
                if (inject_irq(&virtualMachine, IRQ_NUM) < 0) {
                    vm_destroy(&virtualMachine);
                    return EXIT_FAILURE;
                }

                --interruptCount;
            } else {
                virtualMachine.run->request_interrupt_window = 0;
            }
            break;

        case KVM_EXIT_HLT:
            std::cout << "KVM_EXIT_HLT\n";
            stop = true;
            break;

        case KVM_EXIT_SHUTDOWN:
            std::cout << "KVM_EXIT_SHUTDOWN\n";
            stop = true;
            break;

        default:
            std::cout
                << "Unexpected VM exit. Exit code: "
                << virtualMachine.run->exit_reason
                << '\n';

            stop = true;
            break;
        }
    }

    vm_destroy(&virtualMachine);
    return EXIT_SUCCESS;
}
