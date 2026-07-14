#ifndef VM_H
#define VM_H

#include <stddef.h>
#include <stdint.h>
#include <linux/kvm.h>

/*
 * KVM exposes the system, a VM and its vCPU through separate file descriptors.
 * Long mode is configured through CR0, CR3, CR4 and EFER,
 * while regular CPU state such as RIP, RSP and RFLAGS is set before the first KVM_RUN call.
 */

#define GUEST_START_ADDR 0x8000 /* Page tables occupy guest memory below this address */
#define PAGE_SIZE_4K (4u * 1024u)
#define PAGE_SIZE_2M (2u * 1024u * 1024u)
#define PAGE_TABLE_ENTRIES 512u

// Interrupt 1
#define IRQ_NUM   32 /* Entries 0-31 are reserved for CPU exceptions */
#define IRQ_COUNT 2 /* One interrupt assigns the mode and one transfers data */

/* Page table entry flags */
#define PDE64_PRESENT (1u << 0) /* The mapped page or next-level table is present */
#define PDE64_RW      (1u << 1) /* Allows writing to the mapped page */
#define PDE64_USER    (1u << 2) /* Allows access from user mode (ring 3) */
#define PDE64_PS      (1u << 7) /* Maps a 2 MiB page directly from the page directory */

/* Control register and EFER flags */
#define CR0_PE   (1u << 0)  /* CR0: enables protected mode */
#define CR0_PG   (1u << 31) /* CR0: enables paging */
#define CR4_PAE  (1u << 5)  /* CR4: enables the page table format required by long mode */
#define EFER_LME (1u << 8)	/* EFER: enables long mode */
#define EFER_LMA (1u << 10)	/* EFER: indicates that long mode is active */

struct vm {
	int kvm_fd;          /* KVM subsystem */
	int vm_fd;           /* One virtual machine */
	int vcpu_fd;         /* Single virtual CPU created for this VM */
	char *mem;           /* Host mapping of guest physical memory */
	size_t mem_size;
	struct kvm_run *run; /* Shared area containing the reason for a VM exit */
	int run_mmap_size;
};

int  vm_init(struct vm *v, size_t mem_size);
void vm_destroy(struct vm *v);
void setup_long_mode(struct vm *v, struct kvm_sregs *sregs, size_t page_size);
int  load_guest_image(struct vm *v, const char *image_path, uint64_t load_addr);
int  inject_irq(struct vm *v, unsigned int vector);

#endif /* VM_H */
