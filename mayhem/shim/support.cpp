// Host implementations of the non-parsing JKSV/FsLib pieces the fuzzed sources link against:
// logger (silent), error:: helpers, the fslib subset (stdio/POSIX), sys::Task/ProgressTask
// bookkeeping and a synchronous threadpool. None of this is parsing code.
#include "error.hpp"
#include "fslib.hpp"
#include "logging/logger.hpp"
#include "sys/sys.hpp"

#include <cerrno>
#include <cstring>
#include <sys/stat.h>

void logger::initialize() {}
void logger::log(const char *, ...) noexcept {}

bool error::libnx(Result code, const std::source_location &) noexcept { return R_FAILED(code); }
bool error::fslib(bool result, const std::source_location &) noexcept { return !result; }
bool error::is_null(const void *pointer, const std::source_location &) noexcept { return pointer == nullptr; }
bool error::is_null(std::initializer_list<const void *> pointers, const std::source_location &) noexcept
{
    for (const void *p : pointers)
    {
        if (!p) { return true; }
    }
    return false;
}

void fslib::Path::assign(std::string_view path) { m_path.assign(path.data(), path.size()); }

bool fslib::Path::is_valid() const noexcept
{
    const size_t colon = m_path.find(':');
    if (colon == std::string::npos || colon == 0) { return false; }
    if (m_path.size() <= colon + 1) { return false; }
    return m_path.find_first_of("<>:\"|?*", colon + 1) == std::string::npos;
}

fslib::File::File(const fslib::Path &path, uint32_t openMode, int64_t)
{
    const char *mode = "rb";
    if (openMode & FsOpenMode_Create) { mode = "wb"; }
    else if (openMode & FsOpenMode_Append) { mode = "ab"; }
    else if (openMode & FsOpenMode_Write) { mode = "r+b"; }
    m_file = std::fopen(path.c_str(), mode);
}

fslib::File::~File()
{
    if (m_file) { std::fclose(m_file); }
}

int64_t fslib::File::get_size()
{
    const long here = std::ftell(m_file);
    std::fseek(m_file, 0, SEEK_END);
    const long size = std::ftell(m_file);
    std::fseek(m_file, here, SEEK_SET);
    return size;
}

int64_t fslib::File::tell() { return std::ftell(m_file); }
ssize_t fslib::File::read(void *buffer, size_t size) { return std::fread(buffer, 1, size, m_file); }
ssize_t fslib::File::write(const void *buffer, size_t size) { return std::fwrite(buffer, 1, size, m_file); }
fslib::File &fslib::File::operator<<(const char *string)
{
    std::fputs(string, m_file);
    return *this;
}

bool fslib::file_exists(const fslib::Path &path)
{
    struct stat st{};
    return ::stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode);
}

bool fslib::directory_exists(const fslib::Path &path)
{
    struct stat st{};
    return ::stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

bool fslib::create_directories_recursively(const fslib::Path &path)
{
    std::string p = path.string();
    for (size_t i = 1; i <= p.size(); i++)
    {
        if (i == p.size() || p[i] == '/')
        {
            const std::string part = p.substr(0, i);
            if (::mkdir(part.c_str(), 0700) != 0 && errno != EEXIST) { return false; }
        }
    }
    return true;
}

const char *fslib::error::get_string() { return "host fslib error"; }

sys::Task::Task() = default;
sys::Task::Task(sys::threadpool::JobFunction, sys::Task::TaskData) {}
bool sys::Task::is_running() const noexcept { return m_isRunning; }
void sys::Task::complete() noexcept { m_isRunning = false; }
void sys::Task::set_status(std::string_view status) { m_status = status; }
void sys::Task::set_status(std::string &status) { m_status = status; }
std::string sys::Task::get_status() noexcept { return m_status; }
sys::ProgressTask::ProgressTask(sys::threadpool::JobFunction f, sys::ProgressTask::TaskData d) : Task(f, d) {}
void sys::ProgressTask::reset(double goal) noexcept { m_current = 0; m_goal = goal; }
void sys::ProgressTask::update_current(double current) noexcept { m_current = current; }
void sys::ProgressTask::increase_current(double amount) noexcept { m_current += amount; }
double sys::ProgressTask::get_goal() const noexcept { return m_goal; }
double sys::ProgressTask::get_progress() const noexcept { return m_goal > 0 ? m_current / m_goal : 0; }

void sys::threadpool::initialize() {}
void sys::threadpool::exit() {}
void sys::threadpool::push_job(sys::threadpool::JobFunction, sys::threadpool::JobData) {}
