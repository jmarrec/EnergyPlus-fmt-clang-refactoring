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

void demo(const fs::path &filePath) {
    std::cout << EnergyPlus::format("{1}, {0}\n", filePath.generic_string(), 1) << '\n';
}
