/*
 * NULL JSFX - Sampler: the sample files the interface can step through
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Like the JSFX's file slider (slider1:/null_jsfx_samples), the FILE cell steps through the
 * .wav and .flac files in REAPER's Data/null_jsfx_samples folder, plus a null_jsfx_samples
 * folder in the home folder for other hosts. The list is sorted by file name. Any other file
 * can be opened with the file browser.
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

namespace nullsmp {

// (folder: the Synth's wavetables use null_jsfx_wavetables the same way)
inline std::vector<std::string> sampleFolders(const std::string& folder = "null_jsfx_samples")
{
    std::vector<std::string> dirs;
#ifdef _WIN32
    if (const char* a = std::getenv("APPDATA"))
        dirs.push_back(std::string(a) + "\\REAPER\\Data\\" + folder);
    if (const char* h = std::getenv("USERPROFILE"))
        dirs.push_back(std::string(h) + "\\" + folder);
#else
    const char* h = std::getenv("HOME");
    const std::string home = h ? h : "";
# ifdef __APPLE__
    dirs.push_back(home + "/Library/Application Support/REAPER/Data/" + folder);
# else
    const char* x = std::getenv("XDG_CONFIG_HOME");
    dirs.push_back((x && x[0] ? std::string(x) : home + "/.config") + "/REAPER/Data/" + folder);
# endif
    dirs.push_back(home + "/" + folder);
#endif
    return dirs;
}

// .wav or .flac (any case), not hidden
inline bool isSampleFile(const std::string& name)
{
    if (name.empty() || name[0] == '.')
        return false;
    std::string low = name;
    for (char& c : low)
        c = char(std::tolower(static_cast<unsigned char>(c)));
    auto ends = [&low](const char* e) {
        const std::string s(e);
        return low.size() > s.size() && low.compare(low.size() - s.size(), s.size(), s) == 0;
    };
    return ends(".wav") || ends(".flac");
}

// full paths of the sample files, sorted by file name
inline std::vector<std::string> listSampleFiles(const std::string& folder = "null_jsfx_samples")
{
    std::vector<std::pair<std::string, std::string>> found; // (name, path)
    for (const std::string& dir : sampleFolders(folder))
    {
#ifdef _WIN32
        WIN32_FIND_DATAA fd;
        HANDLE hd = FindFirstFileA((dir + "\\*").c_str(), &fd);
        if (hd == INVALID_HANDLE_VALUE)
            continue;
        do
        {
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && isSampleFile(fd.cFileName))
                found.emplace_back(fd.cFileName, dir + "\\" + fd.cFileName);
        } while (FindNextFileA(hd, &fd));
        FindClose(hd);
#else
        DIR* d = opendir(dir.c_str());
        if (!d)
            continue;
        while (dirent* e = readdir(d))
            if (isSampleFile(e->d_name))
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

} // namespace nullsmp
