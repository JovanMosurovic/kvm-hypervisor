#include "file_io.hpp"

#include "file_protocol.h"
#include "vm.h"
#include "vm_runner.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <limits>
#include <string>
#include <string_view>
#include <system_error>
#include <unistd.h>
#include <vector>

namespace {

    struct ResolvedFile {
        std::filesystem::path path;
        bool shared;
    };

    struct ReopenedFile {
        OpenFile *file;
        int hostDescriptor;
    };

    bool isLetter(char character)
    {
        return (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z');
    }

    bool isDigit(char character)
    {
        return character >= '0' && character <= '9';
    }

} // namespace

bool isValidGuestFileName(std::string_view name)
{
    if (name.empty() || !isLetter(name.front())) {
        return false;
    }

    for (char character : name) {
        if (!isLetter(character) && !isDigit(character) && character != '.') {
            return false;
        }
    }

    return true;
}

namespace {

    bool getGuestMemory(struct vm& virtualMachine, std::uint32_t address, std::size_t size, char *&memory)
    {
        const std::size_t guestAddress = address;

        if (guestAddress > virtualMachine.mem_size || size > virtualMachine.mem_size - guestAddress) {
            return false;
        }

        memory = virtualMachine.mem + guestAddress;
        return true;
    }

    bool readGuestFileName(struct vm& virtualMachine, std::uint32_t address, std::string& name)
    {
        char *memory = nullptr;

        if (!getGuestMemory(virtualMachine, address, 1, memory)) {
            return false;
        }

        const std::size_t available = virtualMachine.mem_size - static_cast<std::size_t>(address);

        for (std::size_t length = 0; length <= FILE_NAME_MAX && length < available; ++length) {
            if (memory[length] == '\0') {
                name.assign(memory, length);
                return isValidGuestFileName(name);
            }
        }

        return false;
    }

    bool isAppendFlag(int flags)
    {
        // O_APPEND is 9, which would otherwise look like O_RD | O_CREATE.
        return flags == FILE_OPEN_APPEND;
    }

    bool isTruncateFlag(int flags)
    {
        return (flags & FILE_OPEN_TRUNCATE) != 0;
    }

    bool isExclusiveFlag(int flags)
    {
        return (flags & FILE_OPEN_EXCLUSIVE) != 0;
    }

    bool hasReadAccess(int flags)
    {
        if (isAppendFlag(flags)) {
            return false;
        }

        const int accessMode = flags & (FILE_OPEN_READ | FILE_OPEN_WRITE | FILE_OPEN_READ_WRITE);
        return accessMode == FILE_OPEN_READ || accessMode == FILE_OPEN_READ_WRITE;
    }

    bool hasWriteAccess(int flags)
    {
        if (isAppendFlag(flags)) {
            return true;
        }

        const int accessMode = flags & (FILE_OPEN_READ | FILE_OPEN_WRITE | FILE_OPEN_READ_WRITE);
        return accessMode == FILE_OPEN_WRITE || accessMode == FILE_OPEN_READ_WRITE;
    }

    bool areValidOpenFlags(int flags)
    {
        if (isAppendFlag(flags)) {
            return true;
        }

        constexpr int accessMask = FILE_OPEN_READ | FILE_OPEN_WRITE | FILE_OPEN_READ_WRITE;
        constexpr int allowedMask = accessMask | FILE_OPEN_CREATE | FILE_OPEN_TRUNCATE | FILE_OPEN_EXCLUSIVE;

        if ((flags & ~allowedMask) != 0) {
            return false;
        }

        const int accessMode = flags & accessMask;

        if (accessMode != FILE_OPEN_READ && accessMode != FILE_OPEN_WRITE && accessMode != FILE_OPEN_READ_WRITE) {
            return false;
        }

        if (isExclusiveFlag(flags) && (flags & FILE_OPEN_CREATE) == 0) {
            return false;
        }

        return !isTruncateFlag(flags) || accessMode != FILE_OPEN_READ;
    }

    int toHostOpenFlags(int flags)
    {
        if (isAppendFlag(flags)) {
            return O_WRONLY | O_APPEND;
        }

        int hostFlags = 0;
        const int accessMode = flags & (FILE_OPEN_READ | FILE_OPEN_WRITE | FILE_OPEN_READ_WRITE);

        if (accessMode == FILE_OPEN_READ) {
            hostFlags = O_RDONLY;
        } else if (accessMode == FILE_OPEN_WRITE) {
            hostFlags = O_WRONLY;
        } else {
            hostFlags = O_RDWR;
        }

        if ((flags & FILE_OPEN_CREATE) != 0) {
            hostFlags |= O_CREAT;
        }

        if (isTruncateFlag(flags)) {
            hostFlags |= O_TRUNC;
        }

        if (isExclusiveFlag(flags)) {
            hostFlags |= O_EXCL;
        }

        return hostFlags;
    }

    ResolvedFile resolveFile(const GuestContext& context, std::string_view name)
    {
        const std::filesystem::path localPath =
            std::filesystem::path("vm_files") / ("vm_" + std::to_string(context.id)) / name;

        std::error_code error;

        if (std::filesystem::exists(localPath, error) && !error) {
            return {localPath, false};
        }

        const auto sharedFile = context.sharedState->sharedFiles.find(std::string(name));

        if (sharedFile != context.sharedState->sharedFiles.end()) {
            return {sharedFile->second, true};
        }

        return {localPath, false};
    }

    void closeReopenedFiles(const std::vector<ReopenedFile>& reopenedFiles)
    {
        for (const ReopenedFile& reopenedFile : reopenedFiles) {
            ::close(reopenedFile.hostDescriptor);
        }
    }

    bool reopenSharedFiles(GuestContext& context, std::string_view name, const std::filesystem::path& localPath)
    {
        std::vector<ReopenedFile> reopenedFiles;

        for (auto& [descriptor, openFile] : context.fileState.openFiles) {
            static_cast<void>(descriptor);

            if (!openFile.shared || openFile.name != name) {
                continue;
            }

            const off_t offset = ::lseek(openFile.hostDescriptor, 0, SEEK_CUR);

            if (offset < 0) {
                closeReopenedFiles(reopenedFiles);
                return false;
            }

            const int hostDescriptor = ::open(localPath.c_str(), toHostOpenFlags(openFile.flags), 0644);

            if (hostDescriptor < 0 || ::lseek(hostDescriptor, offset, SEEK_SET) < 0) {
                if (hostDescriptor >= 0) {
                    ::close(hostDescriptor);
                }

                closeReopenedFiles(reopenedFiles);
                return false;
            }

            reopenedFiles.push_back({&openFile, hostDescriptor});
        }

        for (const ReopenedFile& reopenedFile : reopenedFiles) {
            ::close(reopenedFile.file->hostDescriptor);
            reopenedFile.file->hostDescriptor = reopenedFile.hostDescriptor;
            reopenedFile.file->hostPath = localPath.string();
            reopenedFile.file->shared = false;
        }

        return !reopenedFiles.empty();
    }

    bool prepareFileForWrite(GuestContext& context, OpenFile& file)
    {
        if (!file.shared) {
            return true;
        }

        const std::filesystem::path localPath =
            std::filesystem::path("vm_files") / ("vm_" + std::to_string(context.id)) / file.name;

        std::error_code error;
        std::filesystem::create_directories(localPath.parent_path(), error);

        if (error) {
            return false;
        }

        std::filesystem::copy_file(file.hostPath, localPath, error);

        if (error) {
            std::error_code removeError;
            std::filesystem::remove(localPath, removeError);
            return false;
        }

        std::filesystem::permissions(localPath, std::filesystem::perms::owner_write,
                                     std::filesystem::perm_options::add, error);

        if (error || !reopenSharedFiles(context, file.name, localPath)) {
            std::error_code removeError;
            std::filesystem::remove(localPath, removeError);
            return false;
        }

        return true;
    }

    int openFile(GuestContext& context, struct vm& virtualMachine, const file_request& request)
    {
        if (!areValidOpenFlags(request.flags)) {
            return -1;
        }

        std::string name;

        if (!readGuestFileName(virtualMachine, request.buffer, name)) {
            return -1;
        }

        const ResolvedFile resolvedFile = resolveFile(context, name);

        if (resolvedFile.shared && isExclusiveFlag(request.flags)) {
            return -1;
        }

        if (!resolvedFile.shared && (request.flags & FILE_OPEN_CREATE) != 0 && !isAppendFlag(request.flags)) {
            std::error_code error;
            std::filesystem::create_directories(resolvedFile.path.parent_path(), error);

            if (error) {
                return -1;
            }
        }

        const int hostFlags = resolvedFile.shared ? O_RDONLY : toHostOpenFlags(request.flags);
        const int hostDescriptor = ::open(resolvedFile.path.c_str(), hostFlags, 0644);

        if (hostDescriptor < 0) {
            return -1;
        }

        const int descriptor = context.fileState.nextDescriptor++;
        const auto fileIterator = context.fileState.openFiles.emplace(descriptor, OpenFile{
            hostDescriptor,
            request.flags,
            name,
            resolvedFile.path.string(),
            resolvedFile.shared
        }).first;

        if (resolvedFile.shared && isTruncateFlag(request.flags) && !prepareFileForWrite(context, fileIterator->second)) {
            ::close(fileIterator->second.hostDescriptor);
            context.fileState.openFiles.erase(fileIterator);
            return -1;
        }

        return descriptor;
    }

    int closeFile(GuestContext& context, int descriptor)
    {
        const auto file = context.fileState.openFiles.find(descriptor);

        if (file == context.fileState.openFiles.end()) {
            return -1;
        }

        const int result = ::close(file->second.hostDescriptor);
        context.fileState.openFiles.erase(file);
        return result == 0 ? 0 : -1;
    }

    int readFile(GuestContext& context, struct vm& virtualMachine, const file_request& request)
    {
        const auto file = context.fileState.openFiles.find(request.descriptor);

        if (file == context.fileState.openFiles.end() || !hasReadAccess(file->second.flags) || request.count < 0) {
            return -1;
        }

        if (request.count == 0) {
            return 0;
        }

        char *buffer = nullptr;

        if (!getGuestMemory(virtualMachine, request.buffer, static_cast<std::size_t>(request.count), buffer)) {
            return -1;
        }

        const ssize_t result = ::read(file->second.hostDescriptor, buffer, static_cast<std::size_t>(request.count));
        return result < 0 ? -1 : static_cast<int>(result);
    }

    int writeFile(GuestContext& context, struct vm& virtualMachine, const file_request& request)
    {
        const auto file = context.fileState.openFiles.find(request.descriptor);

        if (file == context.fileState.openFiles.end() || !hasWriteAccess(file->second.flags) || request.count < 0) {
            return -1;
        }

        if (!prepareFileForWrite(context, file->second)) {
            return -1;
        }

        if (request.count == 0) {
            return 0;
        }

        char *buffer = nullptr;

        if (!getGuestMemory(virtualMachine, request.buffer, static_cast<std::size_t>(request.count), buffer)) {
            return -1;
        }

        const ssize_t result = ::write(file->second.hostDescriptor, buffer, static_cast<std::size_t>(request.count));
        return result < 0 ? -1 : static_cast<int>(result);
    }

    int seekFile(GuestContext& context, const file_request& request)
    {
        const auto file = context.fileState.openFiles.find(request.descriptor);

        if (file == context.fileState.openFiles.end()) {
            return -1;
        }

        int hostWhence;
        off_t offset;

        if (request.flags == FILE_SEEK_SET) {
            hostWhence = SEEK_SET;
            offset = request.offset;
        } else if (request.flags == FILE_SEEK_END) {
            hostWhence = SEEK_END;
            offset = request.offset;
        } else if (request.flags == FILE_SEEK_CUR) {
            hostWhence = SEEK_CUR;
            offset = request.offset;
        } else {
            return -1;
        }

        const off_t result = ::lseek(file->second.hostDescriptor, offset, hostWhence);

        if (result < 0 || result > std::numeric_limits<int>::max()) {
            return -1;
        }

        return static_cast<int>(result);
    }

    int unlinkFile(GuestContext& context, struct vm& virtualMachine, const file_request& request)
    {
        std::string name;

        if (!readGuestFileName(virtualMachine, request.buffer, name)) {
            return -1;
        }

        const ResolvedFile resolvedFile = resolveFile(context, name);

        if (resolvedFile.shared) {
            return -1;
        }

        std::error_code error;
        const bool removed = std::filesystem::remove(resolvedFile.path, error);
        return removed && !error ? 0 : -1;
    }

    int truncateOpenFile(GuestContext& context, const file_request& request)
    {
        const auto file = context.fileState.openFiles.find(request.descriptor);

        if (file == context.fileState.openFiles.end() || !hasWriteAccess(file->second.flags) || request.offset < 0) {
            return -1;
        }

        if (!prepareFileForWrite(context, file->second)) {
            return -1;
        }

        const int result = ::ftruncate(file->second.hostDescriptor, static_cast<off_t>(request.offset));
        return result == 0 ? 0 : -1;
    }

    int processFileRequest(GuestContext& context, struct vm& virtualMachine, const file_request& request)
    {
        switch (request.operation) {
        case FILE_OPERATION_OPEN:
            return openFile(context, virtualMachine, request);
        case FILE_OPERATION_CLOSE:
            return closeFile(context, request.descriptor);
        case FILE_OPERATION_READ:
            return readFile(context, virtualMachine, request);
        case FILE_OPERATION_WRITE:
            return writeFile(context, virtualMachine, request);
        case FILE_OPERATION_LSEEK:
            return seekFile(context, request);
        case FILE_OPERATION_UNLINK:
            return unlinkFile(context, virtualMachine, request);
        case FILE_OPERATION_FTRUNCATE:
            return truncateOpenFile(context, request);
        default:
            return -1;
        }
    }

} // namespace

bool handleFileIo(GuestContext& context, struct vm& virtualMachine)
{
    const auto& io = virtualMachine.run->io;

    if (io.port != FILE_IO_PORT || io.size != sizeof(std::uint32_t) || io.count != 1) {
        return false;
    }

    const std::size_t dataOffset = static_cast<std::size_t>(io.data_offset);
    const std::size_t mappingSize = static_cast<std::size_t>(virtualMachine.run_mmap_size);

    if (dataOffset > mappingSize || sizeof(std::uint32_t) > mappingSize - dataOffset) {
        return false;
    }

    auto *data = reinterpret_cast<std::uint8_t *>(virtualMachine.run) + dataOffset;

    if (io.direction == KVM_EXIT_IO_OUT) {
        std::uint32_t requestAddress;
        std::memcpy(&requestAddress, data, sizeof(requestAddress));

        char *requestMemory = nullptr;
        std::int32_t result = -1;

        if (getGuestMemory(virtualMachine, requestAddress, sizeof(file_request), requestMemory)) {
            file_request request;
            std::memcpy(&request, requestMemory, sizeof(request));
            result = processFileRequest(context, virtualMachine, request);
        }

        context.fileState.pendingResults.push_back(result);
        return true;
    }

    if (io.direction == KVM_EXIT_IO_IN) {
        std::int32_t result = -1;

        if (!context.fileState.pendingResults.empty()) {
            result = context.fileState.pendingResults.back();
            context.fileState.pendingResults.pop_back();
        }

        std::memcpy(data, &result, sizeof(result));
        return true;
    }

    return false;
}

void closeGuestFiles(GuestContext& context)
{
    for (const auto& [descriptor, file] : context.fileState.openFiles) {
        static_cast<void>(descriptor);
        ::close(file.hostDescriptor);
    }

    context.fileState.openFiles.clear();
}
