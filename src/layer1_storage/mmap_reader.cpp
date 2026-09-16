#include "pqe/layer1_storage/mmap_reader.hpp"

#include <iostream>
#include <system_error>

#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <windows.h>
#else
    #include <sys/mman.h>
    #include <sys/stat.h>
    #include <fcntl.h>
    #include <unistd.h>
#endif

namespace pqe::storage {

#ifdef _WIN32
    struct MmapReader::Impl {
        HANDLE file_handle{INVALID_HANDLE_VALUE};
        HANDLE mapping_handle{NULL};

        void cleanup(const char*& data_ptr, std::size_t& size_ref) noexcept {
            if (data_ptr) {
                UnmapViewOfFile(data_ptr);
                data_ptr = nullptr;
            }
            if (mapping_handle && mapping_handle != INVALID_HANDLE_VALUE) {
                CloseHandle(mapping_handle);
                mapping_handle = NULL;
            }
            if (file_handle && file_handle != INVALID_HANDLE_VALUE) {
                CloseHandle(file_handle);
                file_handle = INVALID_HANDLE_VALUE;
            }
            size_ref = 0;
        }
    };
#else
    struct MmapReader::Impl {
        int fd{-1};

        void cleanup(const char*& data_ptr, std::size_t& size_ref) noexcept {
            if (data_ptr && size_ref > 0) {
                munmap(const_cast<char*>(data_ptr), size_ref);
                data_ptr = nullptr;
            }
            if (fd >= 0) {
                ::close(fd);
                fd = -1;
            }
            size_ref = 0;
        }
    };
#endif

    MmapReader::MmapReader() noexcept 
        : impl_(std::make_unique<Impl>()) {}

    MmapReader::MmapReader(const std::string& filepath)
        : impl_(std::make_unique<Impl>()) {
        open(filepath);
    }

    MmapReader::~MmapReader() {
        close();
    }

    MmapReader::MmapReader(MmapReader&& other) noexcept = default;
    MmapReader& MmapReader::operator=(MmapReader&& other) noexcept = default;

    void MmapReader::close() noexcept {
        if (impl_) {
            impl_->cleanup(data_, size_);
        }
        filepath_.clear();
    }

    bool MmapReader::open(const std::string& filepath) {
        close();
        filepath_ = filepath;

#ifdef _WIN32
        impl_->file_handle = CreateFileA(
            filepath.c_str(),
            GENERIC_READ,
            FILE_SHARE_READ,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_READONLY | FILE_FLAG_SEQUENTIAL_SCAN,
            nullptr
        );

        if (impl_->file_handle == INVALID_HANDLE_VALUE) {
            std::cerr << "[MmapReader] Error: Failed to open file '" << filepath 
                      << "' (Error: " << GetLastError() << ")\n";
            return false;
        }

        LARGE_INTEGER file_size;
        if (!GetFileSizeEx(impl_->file_handle, &file_size)) {
            std::cerr << "[MmapReader] Error: Failed to query file size for '" << filepath << "'\n";
            close();
            return false;
        }

        if (file_size.QuadPart == 0) {
            size_ = 0;
            data_ = "";
            return true;
        }

        size_ = static_cast<std::size_t>(file_size.QuadPart);

        impl_->mapping_handle = CreateFileMappingA(
            impl_->file_handle,
            nullptr,
            PAGE_READONLY,
            0,
            0,
            nullptr
        );

        if (!impl_->mapping_handle) {
            std::cerr << "[MmapReader] Error: CreateFileMapping failed for '" << filepath 
                      << "' (Error: " << GetLastError() << ")\n";
            close();
            return false;
        }

        LPVOID mapped_view = MapViewOfFile(
            impl_->mapping_handle,
            FILE_MAP_READ,
            0,
            0,
            0
        );

        if (!mapped_view) {
            std::cerr << "[MmapReader] Error: MapViewOfFile failed for '" << filepath 
                      << "' (Error: " << GetLastError() << ")\n";
            close();
            return false;
        }

        data_ = static_cast<const char*>(mapped_view);
        advise_sequential();
        return true;

#else
        impl_->fd = ::open(filepath.c_str(), O_RDONLY);
        if (impl_->fd < 0) {
            std::cerr << "[MmapReader] Error: Failed to open file '" << filepath << "'\n";
            return false;
        }

        struct stat sb;
        if (fstat(impl_->fd, &sb) < 0) {
            std::cerr << "[MmapReader] Error: Failed to fstat file '" << filepath << "'\n";
            close();
            return false;
        }

        if (sb.st_size == 0) {
            size_ = 0;
            data_ = "";
            return true;
        }

        size_ = static_cast<std::size_t>(sb.st_size);

        void* mapped = mmap(nullptr, size_, PROT_READ, MAP_PRIVATE, impl_->fd, 0);
        if (mapped == MAP_FAILED) {
            std::cerr << "[MmapReader] Error: POSIX mmap failed for '" << filepath << "'\n";
            close();
            return false;
        }

        data_ = static_cast<const char*>(mapped);
        advise_sequential();
        return true;
#endif
    }

    void MmapReader::advise_sequential() {
        if (!data_ || size_ == 0) return;

#if !defined(_WIN32) && defined(MADV_SEQUENTIAL)
        madvise(const_cast<char*>(data_), size_, MADV_SEQUENTIAL | MADV_WILLNEED);
#endif
    }

} // namespace pqe::storage
