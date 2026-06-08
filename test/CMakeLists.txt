add_executable(path_format_fixer_test PathFormatFixerTest.cpp)
target_link_libraries(path_format_fixer_test
    PRIVATE
        path_format_fixer_lib
        llvm_clang_includes
        gtest::gtest
        fmt::fmt
)

gtest_discover_tests(path_format_fixer_test)
