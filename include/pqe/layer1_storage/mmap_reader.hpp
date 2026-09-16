#pragma once

#include <string>
#include <string_view>
#include <cstddef>
#include <memory>

namespace pqe::storage {

    class MmapReader {
    public:
        MmapReader() noexcept;
        explicit MmapReader(const std::string& filepath);
        ~MmapReader();

        MmapReader(const MmapReader&) = delete;
        MmapReader& operator=(const MmapReader&) = delete;

        MmapReader(MmapReader&& other) noexcept;
        MmapReader& operator=(MmapReader&& other) noexcept;

        bool open(const std::string& filepath);
        void close() noexcept;

        [[nodiscard]] bool is_open() const noexcept { return data_ != nullptr; }
        [[nodiscard]] const char* data() const noexcept { return data_; }
        [[nodiscard]] std::size_t size() const noexcept { return size_; }
        [[nodiscard]] std::string_view string_view() const noexcept {
            return {data_, size_};
        }
        [[nodiscard]] const std::string& filepath() const noexcept { return filepath_; }

        // Advises OS kernel to prefetch pages sequentially into cache
        void advise_sequential();

    private:
        const char* data_{nullptr};
        std::size_t size_{0};
        std::string filepath_{};

        struct Impl;
        std::unique_ptr<Impl> impl_;
    };

} // namespace pqe::storage
