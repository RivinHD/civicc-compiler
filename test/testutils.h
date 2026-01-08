#pragma once

#include "gtest/gtest.h"
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

/// Multiline assert that asserts each line separately for better output.
#define ASSERT_MLSTREQ(expected, value)                                                            \
    do                                                                                             \
    {                                                                                              \
        std::string token_expected;                                                                \
        std::string token_value;                                                                   \
        std::istringstream stream_expected((expected));                                            \
        std::istringstream stream_value((value));                                                  \
        unsigned int relative_line = 1;                                                            \
                                                                                                   \
        while (!std::getline(stream_expected, token_expected).eof() &                              \
               !std::getline(stream_value, token_value).eof())                                     \
        {                                                                                          \
            ASSERT_EQ(token_expected, token_value) << "Line: " << relative_line;                   \
            relative_line++;                                                                       \
        }                                                                                          \
                                                                                                   \
        ASSERT_EQ(token_expected, token_value) << "Line: " << relative_line;                       \
    } while (0)

/// Reads a file to a string relative to the Project directory.
static inline std::string read_file(const std::string &filepath)
{
    const std::filesystem::path project_directory = std::filesystem::absolute(PROJECT_DIRECTORY);
    const std::filesystem::path file = project_directory / filepath;

    constexpr size_t read_size = 4096;
    std::ifstream stream{file};
    stream.exceptions(std::ios_base::badbit);

    if (not stream)
    {
        throw std::ios_base::failure("File does not exist");
    }

    size_t size = std::filesystem::file_size(file);
    std::string out{};
    out.reserve(size);
    std::string buf = std::string(read_size, '\0');
    while (stream.read(&buf[0], read_size))
    {
        out.append(buf, 0, stream.gcount());
    }
    out.append(buf, 0, stream.gcount());
    return out;
}
