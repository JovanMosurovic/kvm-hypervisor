#ifndef KVM_HYPERVISOR_FILE_IO_HPP
#define KVM_HYPERVISOR_FILE_IO_HPP

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

struct GuestContext;
struct vm;

// File system 5
/* Maps a guest descriptor to the real host file opened for that guest. */
struct OpenFile {
    int hostDescriptor = -1;
    int flags = 0;
    std::string name;
    std::string hostPath;
    bool shared = false;
};

struct FileState {
    /* Each VM owns a separate descriptor table. */
    std::unordered_map<int, OpenFile> openFiles;
    std::vector<std::int32_t> pendingResults;
    int nextDescriptor = 3;
};

bool isValidGuestFileName(std::string_view name);
bool handleFileIo(GuestContext& context, struct vm& virtualMachine);
void closeGuestFiles(GuestContext& context);

#endif // KVM_HYPERVISOR_FILE_IO_HPP
