// NULL JSFX: writes the JSFX embedded at build time to a cache file so that
// ysfx (which loads JSFX from a path) can compile it.
#include "null_embedded.h"
#include <cstdint>

extern const unsigned char null_embedded_jsfx[];
extern const std::size_t null_embedded_jsfx_size;

static juce::File cacheRoot()
{
    auto dir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
   #if JUCE_MAC
    dir = dir.getChildFile("Application Support");
   #endif
    return dir.getChildFile(NULL_CACHE_VENDOR);
}

static bool writeIfDifferent(const juce::File &file, const juce::MemoryBlock &data)
{
    if (file.existsAsFile() && file.getSize() == (juce::int64)data.getSize()) {
        juce::MemoryBlock existing;
        if (file.loadFileAsData(existing) && existing == data)
            return true;
    }
    if (!file.getParentDirectory().createDirectory())
        return false;
    juce::TemporaryFile tmp(file);
    if (!tmp.getFile().replaceWithData(data.getData(), data.getSize()))
        return false;
    return tmp.overwriteTargetFileWithTemporary();
}

juce::File nullEmbeddedJsfxFile()
{
    static const juce::File result = [] {
        juce::MemoryBlock data(null_embedded_jsfx, null_embedded_jsfx_size);

        // Content hash in the directory name: different builds never fight
        // over the same file, and an unchanged JSFX is never rewritten.
        uint64_t h = 1469598103934665603ull;  // FNV-1a
        for (std::size_t i = 0; i < null_embedded_jsfx_size; ++i)
            h = (h ^ null_embedded_jsfx[i]) * 1099511628211ull;
        juce::String sub = juce::File(NULL_JSFX_NAME).getFileNameWithoutExtension()
                           + "-" + juce::String::toHexString((juce::int64)h).paddedLeft('0', 16);

        for (auto root : { cacheRoot(), juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile(NULL_CACHE_VENDOR) }) {
            juce::File f = root.getChildFile("embedded").getChildFile(sub).getChildFile(NULL_JSFX_NAME);
            if (writeIfDifferent(f, data))
                return f;
        }
        jassertfalse;
        return juce::File{};
    }();
    return result;
}
