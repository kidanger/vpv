#include <iostream>
#include <regex>

#include <imgui.h>
#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui_internal.h>

#include <reproc++/reproc.hpp>
#include <reproc++/drain.hpp>

#include "events.hpp"
#include "Sequence.hpp"
#include "Player.hpp"
#include "ImageCollection.hpp"
#include "Progressable.hpp"
#include "LoadingThread.hpp"
#include "globals.hpp"
#include "Terminal.hpp"


class Process : public Progressable {
    Terminal& term;
    reproc::process process;
    std::string command;
    bool finished;

public:
    Process(Terminal& term, const std::string& command)
        : term(term), command(command), finished(false) {
        reproc::stop_actions stop = {
            { reproc::stop::terminate, reproc::milliseconds(5000) },
            { reproc::stop::kill, reproc::milliseconds(2000) },
            {}
        };

        reproc::options options;
        options.stop = stop;
        options.redirect.err.type = reproc::redirect::pipe;

        const char* args[] = { "sh", "-c", command.c_str(), nullptr };
        std::error_code ec = process.start(args, options);

        if (ec == std::errc::no_such_file_or_directory) {
            std::cerr << "Program not found. Make sure it's available from the PATH.";
            finished = true;
        } else if (ec) {
            std::cerr << ec.message();
            finished = true;
        }
    }

    float getProgressPercentage() const override {
        return 0.f;
    }

    bool isLoaded() const override {
        return finished;
    }

    void progress() override {
        CommandResult result;
        reproc::sink::string outSink(result.out);
        reproc::sink::string errSink(result.err);

        std::error_code ec = reproc::drain(process, outSink, errSink);
        if (ec) {
            std::cerr << "terminal error (1): " << ec.message();
        } else {
            std::tie(result.status, ec) = process.wait(reproc::infinite);
            if (ec) {
                std::cerr << "terminal error (2): " << ec.message();
            }
        }

        {
            std::lock_guard<std::mutex> _lock(term.lock);
            term.cache[command] = result;
            gActive = std::max(gActive, 2);
            for (auto it = term.queuecommands.begin(); it != term.queuecommands.end(); it++) {
                if (*it == command) {
                    term.queuecommands.erase(it);
                    break;
                }
            }
        }
        finished = true;
    }
};

Terminal::Terminal() {
    runner = new SleepyLoadingThread([&]() -> std::shared_ptr<Progressable> {
        std::lock_guard<std::mutex> _lock(lock);
        if (!queuecommands.empty()) {
            std::string c = queuecommands.front();
            return std::make_shared<Process>(*this, c);
        }
        return nullptr;
    });
    runner->start();
}

void Terminal::setVisible(bool visible) {
    shown = visible;
    focusInput |= visible;
}

static void help(const char* text) {
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::TextUnformatted(text);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

void Terminal::tick() {
    if (!shown)
        return;

    ImGui::SetNextWindowSize(ImVec2(500, 800), ImGuiSetCond_FirstUseEver);
    if (ImGui::Begin("Terminal", &shown, 0)) {
        ImGui::BringFront();
        if ((isKeyPressed("return") && ImGui::IsWindowFocused()) || focusInput)
            ImGui::SetKeyboardFocusHere();
        ImGui::InputText("", bufcommand, sizeof(bufcommand));
        if (!ImGui::GetIO().WantCaptureKeyboard)
            updateOutput();
        ImGui::SameLine();
        if (ImGui::Button(" C ")) {
            std::lock_guard<std::mutex> _lock(lock);
            queuecommands.clear();
            cache.clear();
        }
        help("Clear the result cache and rerun the command.");
        ImGui::BeginChild("..", ImVec2(0,0), false, ImGuiWindowFlags_HorizontalScrollbar);

        if (state == NO_COMMAND) {
        } else {
            if (state == RUNNING) {
                ImGui::TextDisabled("(running...)");
            } else if (state == FINISHED) {
                ImGui::TextDisabled("(status code: %d)", currentResult.status);
            }

            ImGui::Text("stdout:");
            if (!currentResult.out.empty()) {
                ImGui::TextUnformatted(currentResult.out.c_str());
            } else {
                ImGui::TextDisabled("(empty)");
            }

            ImGui::NewLine();
            ImGui::TextUnformatted("sterr:");
            if (!currentResult.err.empty()) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.0f, 0.0f, 1.0f));
                ImGui::TextUnformatted(currentResult.err.c_str());
                ImGui::PopStyleColor();
            } else {
                ImGui::TextDisabled("(empty)");
            }
        }

        ImGui::EndChild();
    }
    ImGui::End();
    focusInput = false;
}

void Terminal::updateOutput() {
    // build the command
    command = bufcommand;
    if (command.empty()) {
        return;
    }
    for (int i = gSequences.size() - 1; i >= 0; i--) {
        const Sequence& seq = *gSequences[i];
        std::string name = seq.collection->getFilename(seq.player->frame-1);
        command = std::regex_replace(command, std::regex("#" + std::to_string(i+1)), name);
    }

    // cache lookup or push to the thread
    std::lock_guard<std::mutex> _lock(lock);
    if (cache.find(command) == cache.end()) {
        for (auto& c : queuecommands) {
            if (c == command) {
                return;
            }
        }
        queuecommands.push_front(command);
        runner->notify();
        state = RUNNING;
    } else {
        currentResult = cache[command];
        state = FINISHED;
    }
}

