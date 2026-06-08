#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>

namespace fs = std::filesystem;

namespace EnergyPlus {
template <typename... Args> std::string format(std::string_view fmt, Args &&...args) {
    return std::string(fmt);
}
} // namespace EnergyPlus

void demo(const fs::path &filePath, const fs::path &other) {
    // positional index
    std::cout << EnergyPlus::format("{1}, {0}\n", filePath.string(), 1) << '\n';
    // two path args, mixed
    std::cout << EnergyPlus::format("a={} b={}\n", filePath.generic_string(), other.string()) << '\n';
    // already has a format spec -> should be skipped with a diagnostic
    std::cout << EnergyPlus::format("{:>20}\n", filePath.generic_string()) << '\n';
    // escaped braces before the placeholder
    std::cout << EnergyPlus::format("{{literal}} {}\n", filePath.string()) << '\n';
}
