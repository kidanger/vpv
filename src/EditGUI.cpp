#include <regex>

#include "imgui.h"
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui_internal.h"

#include "editors.hpp"
#include "Sequence.hpp"
#include "Player.hpp"
#include "globals.hpp"
#include "EditGUI.hpp"

void EditGUI::display(Sequence& seq, bool focus)
{
    if (!isEditing()) {
        return;
    }

    if (focus) {
        ImGui::SetKeyboardFocusHere();
    }

    bool shouldValidate = false;
    const std::string name = getEditorName();
    if (ImGui::InputText(name.c_str(), editprog, sizeof(editprog),
                         ImGuiInputTextFlags_EnterReturnsTrue)) {
        shouldValidate = true;
    }
    if (!isEditing()) {
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
        seq.collection = seq.uneditedCollection;
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

        std::shared_ptr<ImageCollection> collection = create_edited_collection(edittype, prog);
        if (collection) {
            seq.collection = collection;
        }
    }

    seq.forgetImage();
    if (seq.player)
        seq.player->reconfigureBounds();
}

std::string EditGUI::getEditorName() const
{
    switch (edittype) {
        case PLAMBDA:
            return"plambda";
        case GMIC:
            return "gmic";
        case OCTAVE:
            return "octave";
        default:
            return "";
    }
}

