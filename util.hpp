#pragma once
#include <chrono>
#include <string>
#include <vector>
#include <bytell_hash_map.hpp>
#include "spdlog/spdlog.h"

#define STR_(x) #x
#define STR(x) STR_(x)


#ifdef __linux__
#include <unistd.h>
#define _getcwd getcwd
#else
#include <direct.h>
#endif


struct prof
{
    static auto now() { return std::chrono::steady_clock::now(); }
    std::chrono::steady_clock::time_point start_time, end_time;
    void start() {
        start_time = now();
    }
    auto elapsed() const { return end_time - start_time; }
    auto stop() {
        end_time = now();
        return elapsed();
    }

    void print(const std::string& name);
};


//Tag type so we can specialise member functions
template<typename T>
struct identity {
    using type = T;
};

#if defined(WIN32)
#define __builtin_unreachable() __assume(0)
#endif

std::wstring s2ws(const std::string& str);
std::string ws2s(const std::wstring& wstr);

std::vector<char> load_file_contents(std::string const& filepath);

extern std::shared_ptr<spdlog::logger> err_logger;
