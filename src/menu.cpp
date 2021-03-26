#ifdef HAS_GLOB
#include <glob.h>
#endif

#include "imgui.h"

#include "View.hpp"
#include "globals.hpp"
#include "Sequence.hpp"
#include "Window.hpp"
#include "Player.hpp"
#include "Colormap.hpp"
#include "layout.hpp"
#include "menu.hpp"

static bool debug = false;

void menu()
{
    if (debug) ImGui::ShowDemoWindow(&debug);

    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("Players")) {
            for (auto p : gPlayers) {
                if (ImGui::BeginMenu(p->ID.c_str())) {
                    ImGui::Checkbox("Opened", &p->opened);
                    p->displaySettings();
                    ImGui::EndMenu();
                }
            }

            ImGui::Spacing();
            if (ImGui::MenuItem("New player")) {
                Player* p = new Player;
                p->opened = true;
                gPlayers.push_back(p);
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Sequences")) {
            for (auto s : gSequences) {
                if (ImGui::BeginMenu(s->ID.c_str())) {
                    ImGui::Text("%s", s->glob.c_str());

                    if (!s->valid) {
                        ImGui::TextColored(ImVec4(1, 0, 0, 1), "INVALID");
                    }

                    ImGui::Spacing();

                    if (ImGui::Button("Remove current frame")) {
                        s->removeCurrentFrame();
                    }

                    ImGui::Spacing();

                    if (ImGui::CollapsingHeader("Attached player")) {
                        for (auto p : gPlayers) {
                            bool attached = p == s->player;
                            if (ImGui::MenuItem(p->ID.c_str(), nullptr, attached)) {
                                if (s->player)
                                    s->player->reconfigureBounds();
                                s->player = p;
                                s->player->reconfigureBounds();
                            }
                            if (ImGui::BeginPopupContextItem(p->ID.c_str())) {
                                p->displaySettings();
                                ImGui::EndPopup();
                            }
                        }
                    }
                    if (ImGui::CollapsingHeader("Attached view")) {
                        for (auto v : gViews) {
                            bool attached = v == s->view;
                            if (ImGui::MenuItem(v->ID.c_str(), nullptr, attached)) {
                                s->view = v;
                            }
                            if (ImGui::BeginPopupContextItem(v->ID.c_str())) {
                                v->displaySettings();
                                ImGui::EndPopup();
                            }
                        }
                    }
                    if (ImGui::CollapsingHeader("Attached colormap")) {
                        for (auto c : gColormaps) {
                            bool attached = c == s->colormap;
                            if (ImGui::MenuItem(c->ID.c_str(), nullptr, attached)) {
                                s->colormap = c;
                            }
                            if (ImGui::BeginPopupContextItem(c->ID.c_str())) {
                                c->displaySettings();
                                ImGui::EndPopup();
                            }
                        }
                    }

                    ImGui::Spacing();

                    ImGui::PushItemWidth(400);
                    if (ImGui::InputText("File glob", &s->glob_[0], s->glob_.capacity(),
                                         ImGuiInputTextFlags_EnterReturnsTrue)) {
                        strcpy(&s->glob[0], &s->glob_[0]);
                        s->loadFilenames();
                    }
                    if (ImGui::BeginPopupContextItem(s->ID.c_str())) {
                        if (ImGui::Selectable("Reset")) {
                            strcpy(&s->glob_[0], &s->glob[0]);
                        }
                        ImGui::EndPopup();
                    }

#ifdef HAS_GLOB
                    ImGui::BeginChild("scrolling", ImVec2(0, ImGui::GetItemsLineHeightWithSpacing()*5 + 20),
                                      false, ImGuiWindowFlags_HorizontalScrollbar);
                    glob_t res;
                    ::glob(s->glob_.c_str(), GLOB_TILDE, nullptr, &res);
                    for(unsigned int j = 0; j < res.gl_pathc; j++) {
                        if (ImGui::Selectable(res.gl_pathv[j], false)) {
                            strcpy(&s->glob_[0], res.gl_pathv[j]);
                        }
                    }
                    globfree(&res);
                    ImGui::EndChild();
#endif

                    ImGui::EndMenu();
                }
            }

            ImGui::Spacing();
            if (ImGui::MenuItem("Load new sequence")) {
                Sequence* s = new Sequence;
                s->view = gViews[0];
                gSequences.push_back(s);
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Views")) {
            for (auto v : gViews) {
                if (ImGui::BeginMenu(v->ID.c_str())) {
                    v->displaySettings();
                    ImGui::EndMenu();
                }
            }

            ImGui::Spacing();
            if (ImGui::MenuItem("New view")) {
                View* v = new View;
                gViews.push_back(v);
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Windows")) {
            for (auto w : gWindows) {
                if (ImGui::BeginMenu(w->ID.c_str())) {
                    w->displaySettings();
                    ImGui::EndMenu();
                }
            }

            ImGui::Spacing();
            if (ImGui::MenuItem("New window")) {
                Window* w = new Window;
                gWindows.push_back(w);
                relayout(false);
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Colormap")) {
            for (auto c : gColormaps) {
                if (ImGui::BeginMenu(c->ID.c_str())) {
                    c->displaySettings();
                    ImGui::EndMenu();
                }
            }

            ImGui::Spacing();
            if (ImGui::MenuItem("New colormap")) {
                Colormap* c = new Colormap;
                gColormaps.push_back(c);
            }

            ImGui::EndMenu();
        }

        ImGui::Text("Layout: %s", getLayoutName().c_str());
        ImGui::SameLine(); ImGui::ShowHelpMarker("Use Ctrl+L to cycle between layouts.");
        ImGui::EndMainMenuBar();
    }
}

