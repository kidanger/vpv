#include <imgui.h>
#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui_internal.h>

#include "FuzzyFinder.hpp"
#include "ImageCollection.hpp"
#include "Player.hpp"
#include "Sequence.hpp"
#include "doctest.h"
#include "events.hpp"
#include "fuzzy-finder.hpp"

void FuzzyFinderForSequence::extractFilenames(const std::shared_ptr<ImageCollection> col)
{
    filenames = rust::Vec<rust::Str>();
    for (int i = 0; i < col->getLength(); i++) {
        const auto& filename = col->getFilename(i);
        filenames.push_back(filename);
    }
    currentCollection = col;
}

void FuzzyFinderForSequence::open()
{
    focus = true;
    ImGui::OpenPopup("FuzzyFinder");
}

void FuzzyFinderForSequence::display(Sequence& seq)
{
    if (!ImGui::BeginPopup("FuzzyFinder", ImGuiWindowFlags_ResizeFromAnySide))
        return;
    if (isKeyPressed("escape"))
        ImGui::CloseCurrentPopup();

    if (seq.collection != currentCollection.lock()) {
        extractFilenames(seq.collection);
        buf[0] = 0;
        current = 0;
        matches = computeMatches(buf);
    }

    if ((ImGui::IsKeyPressed(getCode("return")) && ImGui::IsWindowFocused()) || focus) {
        ImGui::SetKeyboardFocusHere();
        focus = false;
    }

    if (ImGui::IsKeyPressed(getCode("backspace")) && !buf[0]) {
        ImGui::CloseCurrentPopup();
    }

    if (ImGui::InputText("", buf, sizeof(buf))) {
        matches = computeMatches(buf);
        current = 0;
    }

    bool movedWithKeyboard = false;
    float movedWithKeyboardPosition = 0.1f;
    if (ImGui::IsKeyPressed(getCode("up"))) {
        current = std::max(0, current - 1);
        movedWithKeyboard = true;
    }
    if (ImGui::IsKeyPressed(getCode("down"))) {
        current = std::min((int)matches.size() - 1, current + 1);
        movedWithKeyboard = true;
        movedWithKeyboardPosition = 0.9f;
    }
    if (ImGui::IsKeyPressed(getCode("return")) && current < matches.size()) {
        seq.player->frame = matches[current].index() + 1;
        if (ImGui::GetIO().KeyShift) {
            ImGui::CloseCurrentPopup();
        }
    }

    ImGui::BeginChild("scroll", ImVec2(ImGui::GetWindowContentRegionWidth(), 300), false, ImGuiWindowFlags_HorizontalScrollbar);
    int frame = seq.player->frame - 1;
    int i = 0;
    for (auto& m : matches) {
        bool is_selected = current == i;
        std::string name(m.to_str());
        if (m.index() == frame)
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.35, 0.35, 0.35, 1.0));
        if (ImGui::Selectable(name.c_str(), is_selected, ImGuiSelectableFlags_DontClosePopups)) {
            seq.player->frame = m.index() + 1;
            current = i;
        }
        if (m.index() == frame)
            ImGui::PopStyleColor(1);
        if (is_selected)
            ImGui::SetItemDefaultFocus();
        if (is_selected && movedWithKeyboard && !ImGui::IsItemVisible())
            ImGui::SetScrollHere(movedWithKeyboardPosition);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", name.c_str());
        i++;
    }
    ImGui::EndChild();

    ImGui::EndPopup();
}
