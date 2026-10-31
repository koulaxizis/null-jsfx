/*
 * NULL JSFX - Console Channel (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "ConsoleUI.hpp"

START_NAMESPACE_DISTRHO

class UIConsoleChannel : public UIConsoleBase
{
public:
    UIConsoleChannel() : UIConsoleBase("CONSOLE CHANNEL", false) {}
};

UI* createUI()
{
    return new UIConsoleChannel();
}

END_NAMESPACE_DISTRHO
