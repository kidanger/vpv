#include <imgui.h>
#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui_internal.h>

#include "doctest.h"
#include "murky.hpp"
#include "Sequence.hpp"
#include "Player.hpp"
#include "ImageCollection.hpp"
#include "events.hpp"
#include "FuzzyFinder.hpp"

TEST_CASE("rust fuzzy finder")
{
    auto matcher = murky::fuzzy_string_matcher();
    rust::Vec<rust::Str> values({ "abcd", "test", "a better test case" });

    SUBCASE("")
    {
        const std::string pattern("ab");
        auto matches = matcher->matches(rust::Slice<const rust::Str>(values.data(), values.size()), pattern);
        CHECK(matches.size() == 2);
        CHECK(matches[0].to_str() == "abcd");
        CHECK(matches[1].to_str() == "a better test case");
    }

    SUBCASE("no results")
    {
        const std::string pattern("not a pattern");
        auto matches = matcher->matches(rust::Slice<const rust::Str>(values.data(), values.size()), pattern);
        CHECK(matches.size() == 0);
    }
}

TEST_CASE("fuzzy finder")
{
    FuzzyFinder finder({ "abcd", "test", "a better test case" });

    SUBCASE("")
    {
        const std::string pattern("ab");
        auto matches = finder.matches(pattern);
        CHECK(matches.size() == 2);
        CHECK(matches[0].to_str() == "abcd");
        CHECK(matches[0].index() == 0);
        CHECK(matches[1].to_str() == "a better test case");
        CHECK(matches[1].index() == 2);
    }

    SUBCASE("no results")
    {
        const std::string pattern("not a pattern");
        auto matches = finder.matches(pattern);
        CHECK(matches.size() == 0);
    }
}

void FuzzyFinderGUI::open()
{
    focus = true;
    ImGui::OpenPopup("FuzzyFinder");
}

void FuzzyFinderGUI::display(Sequence& seq)
{
    if (!ImGui::BeginPopup("FuzzyFinder", ImGuiWindowFlags_AlwaysAutoResize))
    //if (!ImGui::BeginPopupModal("FuzzyFinder", 0, ImGuiWindowFlags_AlwaysAutoResize))
        return;
    if (isKeyPressed("escape"))
        ImGui::CloseCurrentPopup();

    if (&seq != currentSequence) {
        const std::shared_ptr<ImageCollection> col = seq.collection;
        std::vector<std::string> filenames;
        for (int i = 0; i < col->getLength(); i++) {
            const auto& filename = col->getFilename(i);
            filenames.push_back(filename);
        }

        finder = std::make_shared<FuzzyFinder>(filenames);
        currentSequence = &seq;
        matches = rust::Vec<murky::IndexedMatch>();
        buf[0] = 0;
        current = 0;
    }

    if ((ImGui::IsKeyPressed(getCode("return")) && ImGui::IsWindowFocused()) || focus) {
        ImGui::SetKeyboardFocusHere();
        focus = false;
    }

    if (ImGui::IsKeyPressed(getCode("backspace")) && !buf[0]) {
        ImGui::CloseCurrentPopup();
    }

    if (ImGui::InputText("", buf, sizeof(buf))) {
        if (buf[0]) {
            matches = finder->matches(buf);
        } else {
            matches = rust::Vec<murky::IndexedMatch>();
        }
        current = 0;
    }

    bool movedWithKeyboard = false;
    float movedWithKeyboardPosition = 0.1f;
    if (ImGui::IsKeyPressed(getCode("up"))) {
        current = std::max(0, current - 1);
        movedWithKeyboard = true;
    }
    if (ImGui::IsKeyPressed(getCode("down"))) {
        current = std::min((int) matches.size() - 1, current + 1);
        movedWithKeyboard = true;
        movedWithKeyboardPosition = 0.9f;
    }
    if (ImGui::IsKeyPressed(getCode("return")) && current < matches.size()) {
        seq.player->frame = matches[current].index() + 1;
        if (ImGui::GetIO().KeyShift) {
            ImGui::CloseCurrentPopup();
        }
    }

    ImGui::ListBoxHeader("", matches.size(), 20);
    int frame = seq.player->frame - 1;
    int i = 0;
    for (auto& m : matches) {
        bool is_selected = m.index() == frame;
        is_selected = current == i;
        std::string name(m.to_str());
        ImGui::PushID(m.index());
        if (ImGui::Selectable(name.c_str(), is_selected, ImGuiSelectableFlags_DontClosePopups)) {
            seq.player->frame = m.index() + 1;
            current = i;
        }
        ImGui::PopID();
        if (is_selected)
            ImGui::SetItemDefaultFocus();
        if (is_selected && movedWithKeyboard && !ImGui::IsItemVisible())
            ImGui::SetScrollHere(movedWithKeyboardPosition);
        i++;
    }
    ImGui::ListBoxFooter();
    ImGui::EndPopup();
}

