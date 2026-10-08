// NULL JSFX: access to the JSFX embedded in this plugin binary.
#pragma once
#include <juce_core/juce_core.h>

// Returns the path of the embedded JSFX, materialising it into a per-user
// cache directory on first use (rewritten only if missing or different).
juce::File nullEmbeddedJsfxFile();
