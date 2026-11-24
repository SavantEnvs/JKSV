// Host stand-in for J-D-K/FsLib (Libraries/FsLib submodule). FsLib wraps the Switch's
// fsp-srv service through libnx, so it cannot run on Linux; this keeps the subset of its
// API the fuzzed sources use, mapped onto stdio/POSIX. "sdmc:/x" paths stay relative paths
// ("sdmc:/x" is a directory named "sdmc:" under the cwd).
#pragma once
#include <switch.h>

#include <cstdio>
#include <string>
#include <string_view>

namespace fslib
{
    class Path final
    {
        public:
            Path() = default;
            Path(const char *path) { assign(path ? std::string_view{path} : std::string_view{}); }
            Path(const std::string &path) { assign(path); }
            Path(std::string_view path) { assign(path); }

            Path &operator=(const char *path) { assign(path ? std::string_view{path} : std::string_view{}); return *this; }
            Path &operator=(const std::string &path) { assign(path); return *this; }
            Path &operator=(std::string_view path) { assign(path); return *this; }

            // FsLib: valid iff a device ("name:") is present, the path after it is non-empty, and
            // it carries no character Horizon's FS rejects.
            bool is_valid() const noexcept;
            std::string string() const { return m_path; }
            const char *c_str() const noexcept { return m_path.c_str(); }

        private:
            std::string m_path{};
            void assign(std::string_view path);
    };

    class File final
    {
        public:
            File() = default;
            File(const fslib::Path &path, uint32_t openMode, int64_t size = 0);
            ~File();
            bool is_open() const noexcept { return m_file != nullptr; }
            operator bool() const noexcept { return is_open(); }
            int64_t get_size();
            int64_t tell();
            ssize_t read(void *buffer, size_t size);
            ssize_t write(const void *buffer, size_t size);
            File &operator<<(const char *string);

        private:
            std::FILE *m_file{};
    };

    bool file_exists(const fslib::Path &path);
    bool directory_exists(const fslib::Path &path);
    bool create_directories_recursively(const fslib::Path &path);

    namespace error
    {
        const char *get_string();
    }
} // namespace fslib
