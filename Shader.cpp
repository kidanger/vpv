#include <string>

#include "imgui.h"

#include "Shader.hpp"

Shader::Shader()
{
    static int id = 0;
    id++;
    ID = "Shader " + std::to_string(id);
}

bool Shader::compile()
{
    return shader.loadFromMemory(codeVertex, std::string(codeTonemap) + codeFragment);
}

void Shader::displaySettings()
{
    ImGui::Text("Name: %s", name.c_str());

    if (ImGui::InputTextMultiline("##source", codeTonemap, SHADER_CODE_SIZE,
                                  ImVec2(600.0f, ImGui::GetTextLineHeight() * 16),
                                  ImGuiInputTextFlags_AllowTabInput)) {
        bool c = compile();
        printf("compilation: %s\n", c ? "ok" : "ko");
    }
}

