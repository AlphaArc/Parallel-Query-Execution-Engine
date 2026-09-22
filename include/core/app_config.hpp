#pragma once

#include <string_view>
#include <string>
#include <cstdint>

namespace pqe::core {

    struct Version {
        std::uint32_t major{0};
        std::uint32_t minor{1};
        std::uint32_t patch{0};

        [[nodiscard]] std::string to_string() const {
            return std::to_string(major) + "." + 
                   std::to_string(minor) + "." + 
                   std::to_string(patch);
        }
    };

    struct AppConfig {
        static constexpr std::string_view ProjectName = "Parallel Query Execution Engine";
        static constexpr std::string_view Environment = "Development";
        static constexpr Version CurrentVersion{0, 1, 0};

        std::string listen_address{"127.0.0.1"};
        std::uint16_t port{8080};
        bool verbose_logging{true};
    };

} // namespace pqe::core
