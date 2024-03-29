#include <glob.h>

#include <imgui.h>

#include "Colormap.hpp"
#include "ImageCollection.hpp"
#include "Player.hpp"
#include "Sequence.hpp"
#include "View.hpp"
#include "Window.hpp"
#include "config.hpp"
#include "globals.hpp"
#include "layout.hpp"
#include "menu.hpp"

static bool debug = false;

void menu()
{
    if (debug)
        ImGui::ShowDemoWindow(&debug);

    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("Players")) {
            for (const auto& p : gPlayers) {
                if (ImGui::BeginMenu(p->ID.c_str())) {
                    ImGui::Checkbox("Opened", &p->opened);
                    p->displaySettings();
                    ImGui::EndMenu();
                }
            }

            ImGui::Spacing();
            if (ImGui::MenuItem("New player")) {
                const auto& p = newPlayer();
                p->opened = true;
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Sequences")) {
            for (const auto& s : gSequences) {
                if (ImGui::BeginMenu(s->ID.c_str())) {
                    const std::string& name = s->getName();
                    ImGui::Text("Identifier: %s", name.c_str());

                    if (!s->valid) {
                        ImGui::TextColored(ImVec4(1, 0, 0, 1), "INVALID");
                    }

                    ImGui::Spacing();

                    if (ImGui::Button("Remove current frame")) {
                        s->removeCurrentFrame();
                    }

                    ImGui::Spacing();

                    if (ImGui::CollapsingHeader("Attached player")) {
                        for (const auto& p : gPlayers) {
                            bool attached = p == s->player;
                            if (ImGui::MenuItem(p->ID.c_str(), nullptr, attached)) {
                                s->attachPlayer(p);
                            }
                            if (ImGui::BeginPopupContextItem(p->ID.c_str())) {
                                p->displaySettings();
                                ImGui::EndPopup();
                            }
                        }
                    }
                    if (ImGui::CollapsingHeader("Attached view")) {
                        for (const auto& v : gViews) {
                            bool attached = v == s->view;
                            if (ImGui::MenuItem(v->ID.c_str(), nullptr, attached)) {
                                s->attachView(v);
                            }
                            if (ImGui::BeginPopupContextItem(v->ID.c_str())) {
                                v->displaySettings();
                                ImGui::EndPopup();
                            }
                        }
                    }
                    if (ImGui::CollapsingHeader("Attached colormap")) {
                        for (const auto& c : gColormaps) {
                            bool attached = c == s->colormap;
                            if (ImGui::MenuItem(c->ID.c_str(), nullptr, attached)) {
                                s->attachColormap(c);
                            }
                            if (ImGui::BeginPopupContextItem(c->ID.c_str())) {
                                c->displaySettings();
                                ImGui::EndPopup();
                            }
                        }
                    }

                    if (ImGui::CollapsingHeader("Image Collection")) {
                        ImGui::BeginChild("scrolling", ImVec2(400, ImGui::GetItemsLineHeightWithSpacing() * 10 + 20),
                            false, ImGuiWindowFlags_HorizontalScrollbar);
                        const ImageCollection& col = *s->collection;
                        for (int i = 0; i < col.getLength(); i++) {
                            const std::string& filename = col.getFilename(i);
                            ImGui::TextUnformatted(filename.c_str());
                        }
                        ImGui::EndChild();
                    }

                    ImGui::EndMenu();
                }
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Views")) {
            for (const auto& v : gViews) {
                if (ImGui::BeginMenu(v->ID.c_str())) {
                    v->displaySettings();
                    ImGui::EndMenu();
                }
            }

            ImGui::Spacing();
            if (ImGui::MenuItem("New view")) {
                newView();
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Windows")) {
            for (const auto& w : gWindows) {
                if (ImGui::BeginMenu(w->ID.c_str())) {
                    w->displaySettings();
                    ImGui::EndMenu();
                }
            }

            ImGui::Spacing();
            if (ImGui::MenuItem("New window")) {
                newWindow();
                relayout();
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Colormap")) {
            for (const auto& c : gColormaps) {
                if (ImGui::BeginMenu(c->ID.c_str())) {
                    c->displaySettings();
                    ImGui::EndMenu();
                }
            }

            ImGui::Spacing();
            if (ImGui::MenuItem("New colormap")) {
                newColormap();
            }

            ImGui::EndMenu();
        }

        ImGui::Text("Layout: %s", getLayoutName().c_str());
        ImGui::SameLine();
        ImGui::ShowHelpMarker("Use Ctrl+L to cycle between layouts.");
        ImGui::EndMainMenuBar();
    }
}
