#include <regex>

#include "imgui.h"
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui_internal.h"

#include "editors.hpp"
#include "Sequence.hpp"
#include "globals.hpp"
#include "EditGUI.hpp"

void EditGUI::display(Sequence& seq, bool focus)
{
    if (!editprog[0]) {
        return;
    }

    if (focus) {
        ImGui::SetKeyboardFocusHere();
    }

    const char* name;
    switch (seq.edittype) {
        case EditType::PLAMBDA: name = "plambda"; break;
        case EditType::GMIC: name = "gmic"; break;
        case EditType::OCTAVE: name = "octave"; break;
    }

    bool shouldValidate = false;
    if (ImGui::InputText(name, editprog, sizeof(editprog),
                         ImGuiInputTextFlags_EnterReturnsTrue)) {
        shouldValidate = true;
    }
    if (!editprog[0]) {
        shouldValidate = true;
    }

    for (int i = 0; i < nvars; i++) {
        std::string name = "$" + std::to_string(i+1);
        if (ImGui::DragFloat(("##"+name).c_str(), &vars[i], 1.f, 0.f, 0.f, (name+": %.3f").c_str())) {
            shouldValidate = true;
        }
    }

    if (shouldValidate) {
        validate(seq);
    }
}

void EditGUI::validate(Sequence& seq)
{
    if (!editprog[0]) {
        seq.setEdit("");
        nvars = 0;
    } else {
        std::string prog(editprog);

        for (nvars = 0; nvars < MAX_VARS; nvars++) {
            std::string n = "$" + std::to_string(nvars+1);
            if (prog.find(n) == std::string::npos)
                break;
        }

        for (int i = 0; i < nvars; i++) {
            std::string val = std::to_string(vars[i]);
            prog = std::regex_replace(prog, std::regex("\\$" + std::to_string(i+1)), val);
        }

        seq.setEdit(prog, seq.edittype);
    }
}

