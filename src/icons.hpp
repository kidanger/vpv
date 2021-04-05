#pragma once

#include <imgui.h>

enum IconID {
    ICON_FIT_COLORMAP,
    ICON_FIT_VIEW,
};

bool show_icon_button(IconID id, const char* description);

