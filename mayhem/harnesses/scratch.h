// Per-process scratch directory: JKSV opens "sdmc:/config/JKSV/..." as given, which on the host is
// a relative path, so the harnesses run JKSV with the cwd inside a mkdtemp() dir under $TMPDIR
// (holding "sdmc:/config/JKSV/"). The cwd is switched only around each input (JksvScratch guard),
// so libFuzzer's own relative corpus/artifact paths keep resolving against the original cwd.
#pragma once
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

static inline std::string &jksv_scratch_dir()
{
    static std::string dir{};
    return dir;
}

static inline void jksv_make_scratch()
{
    const char *tmp  = std::getenv("TMPDIR");
    std::string tmpl = std::string(tmp && *tmp ? tmp : "/tmp") + "/jksv-fuzz-XXXXXX";
    if (!mkdtemp(tmpl.data())) { std::abort(); }
    jksv_scratch_dir() = tmpl;
    for (const char *sub : {"/sdmc:", "/sdmc:/config", "/sdmc:/config/JKSV"}) { mkdir((tmpl + sub).c_str(), 0700); }
}

struct JksvScratch
{
    int oldCwd{-1};
    JksvScratch()
    {
        oldCwd = open(".", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
        if (oldCwd < 0 || chdir(jksv_scratch_dir().c_str()) != 0) { std::abort(); }
    }
    ~JksvScratch()
    {
        if (fchdir(oldCwd) != 0) { std::abort(); }
        close(oldCwd);
    }
};

static inline void jksv_write_file(const char *path, const void *data, size_t size)
{
    FILE *f = std::fopen(path, "wb");
    if (!f) { std::abort(); }
    if (size) { std::fwrite(data, 1, size, f); }
    std::fclose(f);
}
