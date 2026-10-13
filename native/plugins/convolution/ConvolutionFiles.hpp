/*
 * NULL JSFX - Convolution: the IR files the interface can pick
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Like the JSFX's file slider (slider2:/null_jsfx_ir), the IR File cell steps through the .wav
 * files in REAPER's Data/null_jsfx_ir folder, plus a null_jsfx_ir folder in the home folder for
 * other hosts. The list is sorted by file name.
 */

#pragma once

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <string>
#include <vector>

#ifdef _WIN32
# ifndef NOMINMAX
#  define NOMINMAX
# endif
# include <windows.h>
#else
# include <dirent.h>
#endif

namespace nullconv {

inline std::vector<std::string> irFolders()
{
    std::vector<std::string> dirs;
#ifdef _WIN32
    if (const char* a = std::getenv("APPDATA"))
        dirs.push_back(std::string(a) + "\\REAPER\\Data\\null_jsfx_ir");
    if (const char* h = std::getenv("USERPROFILE"))
        dirs.push_back(std::string(h) + "\\null_jsfx_ir");
#else
    const char* h = std::getenv("HOME");
    const std::string home = h ? h : "";
# ifdef __APPLE__
    dirs.push_back(home + "/Library/Application Support/REAPER/Data/null_jsfx_ir");
# else
    const char* x = std::getenv("XDG_CONFIG_HOME");
    dirs.push_back((x && x[0] ? std::string(x) : home + "/.config") + "/REAPER/Data/null_jsfx_ir");
# endif
    dirs.push_back(home + "/null_jsfx_ir");
#endif
    return dirs;
}

inline bool isWav(const std::string& name)
{
    if (name.size() < 5 || name[0] == '.')
        return false;
    std::string ext = name.substr(name.size() - 4);
    for (char& c : ext)
        c = char(std::tolower(static_cast<unsigned char>(c)));
    return ext == ".wav";
}

// full paths of the IR files, sorted by file name
inline std::vector<std::string> listIrFiles()
{
    std::vector<std::pair<std::string, std::string>> found; // (name, path)
    for (const std::string& dir : irFolders())
    {
#ifdef _WIN32
        WIN32_FIND_DATAA fd;
        HANDLE hd = FindFirstFileA((dir + "\\*.wav").c_str(), &fd);
        if (hd == INVALID_HANDLE_VALUE)
            continue;
        do
        {
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && isWav(fd.cFileName))
                found.emplace_back(fd.cFileName, dir + "\\" + fd.cFileName);
        } while (FindNextFileA(hd, &fd));
        FindClose(hd);
#else
        DIR* d = opendir(dir.c_str());
        if (!d)
            continue;
        while (dirent* e = readdir(d))
            if (isWav(e->d_name))
                found.emplace_back(e->d_name, dir + "/" + e->d_name);
        closedir(d);
#endif
    }
    std::sort(found.begin(), found.end());
    std::vector<std::string> paths;
    for (auto& f : found)
        paths.push_back(f.second);
    return paths;
}

} // namespace nullconv
