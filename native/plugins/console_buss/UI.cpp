/*
 * NULL JSFX - Console Buss (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "../console_channel/ConsoleUI.hpp"

START_NAMESPACE_DISTRHO

class UIConsoleBuss : public UIConsoleBase
{
public:
    UIConsoleBuss() : UIConsoleBase("CONSOLE BUSS", true) {}
};

UI* createUI()
{
    return new UIConsoleBuss();
}

END_NAMESPACE_DISTRHO
