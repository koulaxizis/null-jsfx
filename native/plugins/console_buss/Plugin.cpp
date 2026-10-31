/*
 * NULL JSFX - Console Buss (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "../console_channel/ConsolePlugin.hpp"

START_NAMESPACE_DISTRHO

class PluginConsoleBuss : public PluginConsoleBase
{
public:
    PluginConsoleBuss() : PluginConsoleBase(true) {}

protected:
    const char* getLabel() const override { return "ConsoleBuss"; }
    const char* getDescription() const override
    {
        return "A mixing-console summing bus: soft saturation with headroom in four characters and a little crosstalk between the sides.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', 'C', 'B', 'u'); }

private:
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginConsoleBuss)
};

Plugin* createPlugin()
{
    return new PluginConsoleBuss();
}

END_NAMESPACE_DISTRHO
