#include <regex>

#include <imgui.h>
#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui_internal.h>

#include "EditGUI.hpp"
#include "Player.hpp"
#include "Sequence.hpp"
#include "editors.hpp"
#include "globals.hpp"

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

    {
        std::array<char, 4096> tmpprog;
        size_t n = std::min(editprog.size(), tmpprog.size() - 1);
        std::copy(editprog.cbegin(), editprog.cbegin() + n, tmpprog.begin());
        tmpprog[n] = 0;
        if (ImGui::InputText(name.c_str(), tmpprog.data(), tmpprog.size(), ImGuiInputTextFlags_EnterReturnsTrue)) {
            shouldValidate = true;
        }
        editprog = std::string(tmpprog.data());
    }

    if (!isEditing()) {
        shouldValidate = true;
    }

    for (int i = 0; i < nvars; i++) {
        std::string name = "$" + std::to_string(i + 1);
        if (ImGui::DragFloat(("##" + name).c_str(), &vars[i], 1.f, 0.f, 0.f, (name + ": %.3f").c_str())) {
            shouldValidate = true;
        }
    }

    if (shouldValidate) {
        validate(seq);
    }
}

void EditGUI::validate(Sequence& seq)
{
    if (editprog.empty()) {
        seq.collection = seq.uneditedCollection;
        nvars = 0;
    } else {
        std::string prog(editprog);

        for (nvars = 0; nvars < MAX_VARS; nvars++) {
            std::string n = "$" + std::to_string(nvars + 1);
            if (prog.find(n) == std::string::npos)
                break;
        }

        for (int i = 0; i < nvars; i++) {
            std::string val = std::to_string(vars[i]);
            prog = std::regex_replace(prog, std::regex("\\$" + std::to_string(i + 1)), val);
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
        return "plambda";
    case OCTAVE:
        return "octave";
    default:
        return "";
    }
}
