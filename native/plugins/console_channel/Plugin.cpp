/*
 * NULL JSFX - Console Channel (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "ConsolePlugin.hpp"

START_NAMESPACE_DISTRHO

class PluginConsoleChannel : public PluginConsoleBase
{
public:
    PluginConsoleChannel() : PluginConsoleBase(false) {}

protected:
    const char* getLabel() const override { return "ConsoleChannel"; }
    const char* getDescription() const override
    {
        return "A mixing-console channel colour: pre-emphasis, soft saturation with headroom and the inverse shelf, in four characters.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', 'C', 'C', 'h'); }

private:
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginConsoleChannel)
};

Plugin* createPlugin()
{
    return new PluginConsoleChannel();
}

END_NAMESPACE_DISTRHO
