// ImGui SDL2 binding with OpenGL3
// (SDL is a cross-platform general purpose library for handling windows, inputs, OpenGL/Vulkan graphics context creation, etc.)
// (GL3W is a helper library to access OpenGL functions since there is no standard header to access modern OpenGL functions easily. Alternatives are GLEW, Glad, etc.)

// Implemented features:
//  [X] User texture binding. Cast 'GLuint' OpenGL texture identifier as void*/ImTextureID. Read the FAQ about ImTextureID in imgui.cpp.
// Missing features:
//  [ ] SDL2 handling of IME under Windows appears to be broken and it explicitly disable the regular Windows IME. You can restore Windows IME by compiling SDL with SDL_DISABLE_WINDOWS_IME.

// You can copy and use unmodified imgui_impl_* files in your project. See main.cpp for an example of using this.
// If you use this binding you'll need to call 4 functions: ImGui_ImplXXXX_Init(), ImGui_ImplXXXX_NewFrame(), ImGui::Render() and ImGui_ImplXXXX_Shutdown().
// If you are new to ImGui, see examples/README.txt and documentation at the top of imgui.cpp.
// https://github.com/ocornut/imgui

// CHANGELOG
// (minor and older changes stripped away, please see git history for details)
//  2018-03-20: Misc: Setup io.BackendFlags ImGuiBackendFlags_HasMouseCursors flag + honor ImGuiConfigFlags_NoMouseCursorChange flag.
//  2018-03-06: OpenGL: Added const char* glsl_version parameter to ImGui_ImplSdlGL3_Init() so user can override the GLSL version e.g. "#version 150".
//  2018-02-23: OpenGL: Create the VAO in the render function so the setup can more easily be used with multiple shared GL context.
//  2018-02-16: Inputs: Added support for mouse cursors, honoring ImGui::GetMouseCursor() value.
//  2018-02-16: Misc: Obsoleted the io.RenderDrawListsFn callback and exposed ImGui_ImplSdlGL3_RenderDrawData() in the .h file so you can call it yourself.
//  2018-02-06: Misc: Removed call to ImGui::Shutdown() which is not available from 1.60 WIP, user needs to call CreateContext/DestroyContext themselves.
//  2018-02-06: Inputs: Added mapping for ImGuiKey_Space.
//  2018-02-05: Misc: Using SDL_GetPerformanceCounter() instead of SDL_GetTicks() to be able to handle very high framerate (1000+ FPS).
//  2018-02-05: Inputs: Keyboard mapping is using scancodes everywhere instead of a confusing mixture of keycodes and scancodes.
//  2018-01-20: Inputs: Added Horizontal Mouse Wheel support.
//  2018-01-19: Inputs: When available (SDL 2.0.4+) using SDL_CaptureMouse() to retrieve coordinates outside of client area when dragging. Otherwise (SDL 2.0.3 and before) testing for SDL_WINDOW_INPUT_FOCUS instead of SDL_WINDOW_MOUSE_FOCUS.
//  2018-01-18: Inputs: Added mapping for ImGuiKey_Insert.
//  2018-01-07: OpenGL: Changed GLSL shader version from 330 to 150.
//  2017-09-01: OpenGL: Save and restore current bound sampler. Save and restore current polygon mode.
//  2017-08-25: Inputs: MousePos set to -FLT_MAX,-FLT_MAX when mouse is unavailable/missing (instead of -1,-1).
//  2017-05-01: OpenGL: Fixed save and restore of current blend func state.
//  2017-05-01: OpenGL: Fixed save and restore of current GL_ACTIVE_TEXTURE.
//  2016-10-15: Misc: Added a void* user_data parameter to Clipboard function handlers.
//  2016-09-05: OpenGL: Fixed save and restore of current scissor rectangle.
//  2016-07-29: OpenGL: Explicitly setting GL_UNPACK_ROW_LENGTH to reduce issues because SDL changes it. (#752)

#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "imgui.h"
#include "imgui_impl_sdl_gl3.h"

// SDL,GL3W
#include <SDL3/SDL.h>
#include <GL/gl3w.h>    // This example is using gl3w to access OpenGL functions (because it is small). You may use glew/glad/glLoadGen/etc. whatever already works for you.
#include "../../src/Shader.hpp"

static const char* getGLError(GLenum error)
{
#define casereturn(x) case x: return #x
    switch (error) {
        casereturn(GL_INVALID_ENUM);
        casereturn(GL_INVALID_VALUE);
        casereturn(GL_INVALID_OPERATION);
        casereturn(GL_INVALID_FRAMEBUFFER_OPERATION);
        case GL_OUT_OF_MEMORY: fprintf(stderr, "%s:%d: out of memory\n", __FILE__, __LINE__); exit(1);
        default:
        case GL_NO_ERROR:
        return "";
    }
#undef casereturn
}

#define GLDEBUG() \
{ \
    GLenum e; \
    while((e = glGetError()) != GL_NO_ERROR) \
    { \
        printf("%s:%s:%d\n", getGLError(e), __FILE__, __LINE__); exit(0); \
    } \
}

// SDL data
static Uint64       g_Time = 0;
static bool         g_MousePressed[3] = { false, false, false };
static SDL_Cursor*  g_MouseCursors[ImGuiMouseCursor_COUNT] = { 0 };

// OpenGL data
static char         g_GlslVersion[32] = "#version 150";
static GLuint       g_FontTexture = 0;
static int          g_ShaderHandle = 0;
static int          g_AttribLocationTex = 0, g_AttribLocationProjMtx = 0;
static int          g_AttribLocationPosition = 0, g_AttribLocationUV = 0, g_AttribLocationColor = 0;
static unsigned int g_VboHandle = 0,g_ElementsHandle = 0;

Shader::Program * g_shader;

// This is the main rendering function that you have to implement and provide to ImGui (via setting up 'RenderDrawListsFn' in the ImGuiIO structure)
// Note that this implementation is little overcomplicated because we are saving/setting up/restoring every OpenGL state explicitly, in order to be able to run within any OpenGL engine that doesn't do so.
// If text or lines are blurry when integrating ImGui in your engine: in your Render function, try translating your projection matrix by (0.5f,0.5f) or (0.375f,0.375f)
void ImGui_ImplSdlGL3_RenderDrawData(ImDrawData* draw_data)
{
    // Avoid rendering when minimized, scale coordinates for retina displays (screen coordinates != framebuffer coordinates)
    ImGuiIO& io = ImGui::GetIO();
    int fb_width = (int)(io.DisplaySize.x * io.DisplayFramebufferScale.x);
    int fb_height = (int)(io.DisplaySize.y * io.DisplayFramebufferScale.y);
    if (fb_width == 0 || fb_height == 0)
        return;
    draw_data->ScaleClipRects(io.DisplayFramebufferScale);

    // Backup GL state
    GLenum last_active_texture; glGetIntegerv(GL_ACTIVE_TEXTURE, (GLint*)&last_active_texture);
    glActiveTexture(GL_TEXTURE0);
    GLint last_program; glGetIntegerv(GL_CURRENT_PROGRAM, &last_program);
    GLint last_texture; glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_texture);
    GLint last_sampler; glGetIntegerv(GL_SAMPLER_BINDING, &last_sampler);
    GLint last_array_buffer; glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &last_array_buffer);
    GLint last_element_array_buffer; glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &last_element_array_buffer);
    GLint last_vertex_array; glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &last_vertex_array);
    GLint last_polygon_mode[2]; glGetIntegerv(GL_POLYGON_MODE, last_polygon_mode);
    GLint last_viewport[4]; glGetIntegerv(GL_VIEWPORT, last_viewport);
    GLint last_scissor_box[4]; glGetIntegerv(GL_SCISSOR_BOX, last_scissor_box);
    GLenum last_blend_src_rgb; glGetIntegerv(GL_BLEND_SRC_RGB, (GLint*)&last_blend_src_rgb);
    GLenum last_blend_dst_rgb; glGetIntegerv(GL_BLEND_DST_RGB, (GLint*)&last_blend_dst_rgb);
    GLenum last_blend_src_alpha; glGetIntegerv(GL_BLEND_SRC_ALPHA, (GLint*)&last_blend_src_alpha);
    GLenum last_blend_dst_alpha; glGetIntegerv(GL_BLEND_DST_ALPHA, (GLint*)&last_blend_dst_alpha);
    GLenum last_blend_equation_rgb; glGetIntegerv(GL_BLEND_EQUATION_RGB, (GLint*)&last_blend_equation_rgb);
    GLenum last_blend_equation_alpha; glGetIntegerv(GL_BLEND_EQUATION_ALPHA, (GLint*)&last_blend_equation_alpha);
    GLboolean last_enable_blend = glIsEnabled(GL_BLEND);
    GLboolean last_enable_cull_face = glIsEnabled(GL_CULL_FACE);
    GLboolean last_enable_depth_test = glIsEnabled(GL_DEPTH_TEST);
    GLboolean last_enable_scissor_test = glIsEnabled(GL_SCISSOR_TEST);

    // Setup render state: alpha-blending enabled, no face culling, no depth testing, scissor enabled, polygon fill
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_SCISSOR_TEST);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    GLDEBUG();

    // Setup viewport, orthographic projection matrix
    glViewport(0, 0, (GLsizei)fb_width, (GLsizei)fb_height);
    const float ortho_projection[4][4] =
    {
        { 2.0f/io.DisplaySize.x, 0.0f,                   0.0f, 0.0f },
        { 0.0f,                  2.0f/-io.DisplaySize.y, 0.0f, 0.0f },
        { 0.0f,                  0.0f,                  -1.0f, 0.0f },
        {-1.0f,                  1.0f,                   0.0f, 1.0f },
    };
    g_shader->bind();
    glUniform1i(g_AttribLocationTex, 0);
    glUniformMatrix4fv(g_AttribLocationProjMtx, 1, GL_FALSE, &ortho_projection[0][0]);
    glBindSampler(0, 0); // Rely on combined texture/sampler state.
    GLDEBUG();

    // Recreate the VAO every time
    // (This is to easily allow multiple GL contexts. VAO are not shared among GL contexts, and we don't track creation/deletion of windows so we don't have an obvious key to use to cache them.)
    GLuint vao_handle = 0;
    glGenVertexArrays(1, &vao_handle);
    glBindVertexArray(vao_handle);
    glBindBuffer(GL_ARRAY_BUFFER, g_VboHandle);
    GLDEBUG();
    glEnableVertexAttribArray(g_AttribLocationPosition);
    GLDEBUG();
    glEnableVertexAttribArray(g_AttribLocationUV);
    GLDEBUG();
    glEnableVertexAttribArray(g_AttribLocationColor);
    GLDEBUG();
    glVertexAttribPointer(g_AttribLocationPosition, 2, GL_FLOAT, GL_FALSE, sizeof(ImDrawVert), (GLvoid*)IM_OFFSETOF(ImDrawVert, pos));
    glVertexAttribPointer(g_AttribLocationUV, 2, GL_FLOAT, GL_FALSE, sizeof(ImDrawVert), (GLvoid*)IM_OFFSETOF(ImDrawVert, uv));
    glVertexAttribPointer(g_AttribLocationColor, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(ImDrawVert), (GLvoid*)IM_OFFSETOF(ImDrawVert, col));
    GLDEBUG();

    // Draw
    for (int n = 0; n < draw_data->CmdListsCount; n++)
    {
        const ImDrawList* cmd_list = draw_data->CmdLists[n];
        const ImDrawIdx* idx_buffer_offset = 0;

        glBindBuffer(GL_ARRAY_BUFFER, g_VboHandle);
        glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)cmd_list->VtxBuffer.Size * sizeof(ImDrawVert), (const GLvoid*)cmd_list->VtxBuffer.Data, GL_STREAM_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_ElementsHandle);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx), (const GLvoid*)cmd_list->IdxBuffer.Data, GL_STREAM_DRAW);
        GLDEBUG();

        for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++)
        {
            const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];

            if (pcmd->UserCallback)
            {
                pcmd->UserCallback(cmd_list, pcmd);
                GLDEBUG();
                GLint p; glGetIntegerv(GL_CURRENT_PROGRAM, &p);
                auto u = glGetUniformLocation(p, "v_transform");
                glUniformMatrix4fv(u, 1, GL_FALSE, &ortho_projection[0][0]);
                GLDEBUG();
            }
            else
            {
                GLDEBUG();
                glBindTexture(GL_TEXTURE_2D, (GLuint)(intptr_t)pcmd->TextureId);
                glScissor((int)pcmd->ClipRect.x, (int)(fb_height - pcmd->ClipRect.w), (int)(pcmd->ClipRect.z - pcmd->ClipRect.x), (int)(pcmd->ClipRect.w - pcmd->ClipRect.y));
                glDrawElements(GL_TRIANGLES, (GLsizei)pcmd->ElemCount, sizeof(ImDrawIdx) == 2 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT, idx_buffer_offset);
                GLDEBUG();
            }
            idx_buffer_offset += pcmd->ElemCount;
        }
    }
    glDeleteVertexArrays(1, &vao_handle);

    // Restore modified GL state
    glUseProgram(last_program);
    glBindTexture(GL_TEXTURE_2D, last_texture);
    glBindSampler(0, last_sampler);
    glActiveTexture(last_active_texture);
    glBindVertexArray(last_vertex_array);
    glBindBuffer(GL_ARRAY_BUFFER, last_array_buffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, last_element_array_buffer);
    glBlendEquationSeparate(last_blend_equation_rgb, last_blend_equation_alpha);
    glBlendFuncSeparate(last_blend_src_rgb, last_blend_dst_rgb, last_blend_src_alpha, last_blend_dst_alpha);
    if (last_enable_blend) glEnable(GL_BLEND); else glDisable(GL_BLEND);
    if (last_enable_cull_face) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
    if (last_enable_depth_test) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
    if (last_enable_scissor_test) glEnable(GL_SCISSOR_TEST); else glDisable(GL_SCISSOR_TEST);
    glPolygonMode(GL_FRONT_AND_BACK, (GLenum)last_polygon_mode[0]);
    glViewport(last_viewport[0], last_viewport[1], (GLsizei)last_viewport[2], (GLsizei)last_viewport[3]);
    glScissor(last_scissor_box[0], last_scissor_box[1], (GLsizei)last_scissor_box[2], (GLsizei)last_scissor_box[3]);
    GLDEBUG();
}

static const char* ImGui_ImplSdlGL3_GetClipboardText(void*)
{
    return SDL_GetClipboardText();
}

static void ImGui_ImplSdlGL3_SetClipboardText(void*, const char* text)
{
    SDL_SetClipboardText(text);
}

// You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
// - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application.
// - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application.
// Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
bool ImGui_ImplSdlGL3_ProcessEvent(SDL_Event* event)
{
    ImGuiIO& io = ImGui::GetIO();
    switch (event->type)
    {
    case SDL_EVENT_MOUSE_WHEEL:
        {
            if (event->wheel.x > 0) io.MouseWheelH += 1;
            if (event->wheel.x < 0) io.MouseWheelH -= 1;
            if (event->wheel.y > 0) io.MouseWheel += 1;
            if (event->wheel.y < 0) io.MouseWheel -= 1;
            return true;
        }
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        {
            if (event->button.button == SDL_BUTTON_LEFT) g_MousePressed[0] = true;
            if (event->button.button == SDL_BUTTON_RIGHT) g_MousePressed[1] = true;
            if (event->button.button == SDL_BUTTON_MIDDLE) g_MousePressed[2] = true;
            return true;
        }
    case SDL_EVENT_TEXT_INPUT:
        {
            io.AddInputCharactersUTF8(event->text.text);
            return true;
        }
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP:
        {
            SDL_Keycode key = event->key.key & ~SDLK_SCANCODE_MASK;
            IM_ASSERT(key >= 0 && key < IM_ARRAYSIZE(io.KeysDown));
            io.KeysDown[key] = (event->type == SDL_EVENT_KEY_DOWN);
            io.KeyShift = ((SDL_GetModState() & SDL_KMOD_SHIFT) != 0);
            io.KeyCtrl = ((SDL_GetModState() & SDL_KMOD_CTRL) != 0);
            io.KeyAlt = ((SDL_GetModState() & SDL_KMOD_ALT) != 0);
            io.KeySuper = ((SDL_GetModState() & SDL_KMOD_GUI) != 0);
            return true;
        }
    default:
        if (event->type >= SDL_EVENT_WINDOW_FIRST and event->type <= SDL_EVENT_WINDOW_LAST) {
            SDL_Window* window = SDL_GetWindowFromID(event->window.windowID);
            int w, h;
            int display_w, display_h;
            SDL_GetWindowSize(window, &w, &h);
            SDL_GetWindowSizeInPixels(window, &display_w, &display_h);
            io.DisplaySize = ImVec2((float)w, (float)h);
            io.DisplayFramebufferScale = ImVec2(w > 0 ? ((float)display_w / w) : 0, h > 0 ? ((float)display_h / h) : 0);
        }
        break;
    }
    return false;
}

static void ImGui_ImplSdlGL3_CreateFontsTexture()
{
    // Build texture atlas
    ImGuiIO& io = ImGui::GetIO();
    unsigned char* pixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);   // Load as RGBA 32-bits for OpenGL3 demo because it is more likely to be compatible with user's existing shader.

    // Upload texture to graphics system
    GLint last_texture;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_texture);
    glGenTextures(1, &g_FontTexture);
    glBindTexture(GL_TEXTURE_2D, g_FontTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

    // Store our identifier
    io.Fonts->TexID = (void *)(intptr_t)g_FontTexture;

    // Restore state
    glBindTexture(GL_TEXTURE_2D, last_texture);
}

bool ImGui_ImplSdlGL3_CreateDeviceObjects()
{
    // Backup GL state
    GLint last_texture, last_array_buffer, last_vertex_array;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_texture);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &last_array_buffer);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &last_vertex_array);

    const GLchar *vertex_shader =
        "uniform mat4 v_transform;\n"
        "layout(location=1) in vec2 v_position;\n"
        "layout(location=2) in vec2 v_texcoord;\n"
        "layout(location=3) in vec4 v_color;\n"
        "out vec2 f_texcoord;\n"
        "out vec4 f_color;\n"
        "void main()\n"
        "{\n"
        "	f_texcoord = v_texcoord;\n"
        "	f_color = v_color;\n"
        "	gl_Position = v_transform * vec4(v_position.xy,0,1);\n"
        "}\n";

    const GLchar* fragment_shader =
        "uniform sampler2D tex;\n"
        "in vec2 f_texcoord;\n"
        "in vec4 f_color;\n"
        "out vec4 out_color;\n"
        "void main()\n"
        "{\n"
        "	out_color = f_color * texture(tex, f_texcoord.st);\n"
        "}\n";

    g_shader = new Shader::Program({Shader::Vertex(vertex_shader), Shader::Fragment(fragment_shader)});
    GLDEBUG();
    g_ShaderHandle = g_shader->getProgramID();

    g_AttribLocationTex = glGetUniformLocation(g_ShaderHandle, "tex");
    g_AttribLocationProjMtx = glGetUniformLocation(g_ShaderHandle, "v_transform");
    GLDEBUG();
    g_AttribLocationPosition = glGetAttribLocation(g_ShaderHandle, "v_position");
    g_AttribLocationUV = glGetAttribLocation(g_ShaderHandle, "v_texcoord");
    g_AttribLocationColor = glGetAttribLocation(g_ShaderHandle, "v_color");
    GLDEBUG();

    glGenBuffers(1, &g_VboHandle);
    glGenBuffers(1, &g_ElementsHandle);
    GLDEBUG();

    ImGui_ImplSdlGL3_CreateFontsTexture();
    GLDEBUG();

    // Restore modified GL state
    glBindTexture(GL_TEXTURE_2D, last_texture);
    glBindBuffer(GL_ARRAY_BUFFER, last_array_buffer);
    glBindVertexArray(last_vertex_array);

    return true;
}

void    ImGui_ImplSdlGL3_InvalidateDeviceObjects()
{
    if (g_VboHandle) glDeleteBuffers(1, &g_VboHandle);
    if (g_ElementsHandle) glDeleteBuffers(1, &g_ElementsHandle);
    g_VboHandle = g_ElementsHandle = 0;

    if (g_shader) {
        delete g_shader;
        g_shader = nullptr;
    }

    if (g_FontTexture)
    {
        glDeleteTextures(1, &g_FontTexture);
        ImGui::GetIO().Fonts->TexID = 0;
        g_FontTexture = 0;
    }
}

bool    ImGui_ImplSdlGL3_Init(SDL_Window* window, const char* glsl_version)
{
    // Store GL version string so we can refer to it later in case we recreate shaders.
    if (glsl_version == NULL)
        glsl_version = "#version 150";
    IM_ASSERT((int)strlen(glsl_version) + 2 < IM_ARRAYSIZE(g_GlslVersion));
    strcpy(g_GlslVersion, glsl_version);
    strcat(g_GlslVersion, "\n");

    // Setup back-end capabilities flags
    ImGuiIO& io = ImGui::GetIO();
    io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;   // We can honor GetMouseCursor() values (optional)

    // Keyboard mapping. ImGui will use those indices to peek into the io.KeysDown[] array.
#define M(k) ((k) & ~SDLK_SCANCODE_MASK)
    io.KeyMap[ImGuiKey_Tab] = M(SDLK_TAB);
    io.KeyMap[ImGuiKey_LeftArrow] = M(SDLK_LEFT);
    io.KeyMap[ImGuiKey_RightArrow] = M(SDLK_RIGHT);
    io.KeyMap[ImGuiKey_UpArrow] = M(SDLK_UP);
    io.KeyMap[ImGuiKey_DownArrow] = M(SDLK_DOWN);
    io.KeyMap[ImGuiKey_PageUp] = M(SDLK_PAGEUP);
    io.KeyMap[ImGuiKey_PageDown] = M(SDLK_PAGEDOWN);
    io.KeyMap[ImGuiKey_Home] = M(SDLK_HOME);
    io.KeyMap[ImGuiKey_End] = M(SDLK_END);
    io.KeyMap[ImGuiKey_Insert] = M(SDLK_INSERT);
    io.KeyMap[ImGuiKey_Delete] = M(SDLK_DELETE);
    io.KeyMap[ImGuiKey_Backspace] = M(SDLK_BACKSPACE);
    io.KeyMap[ImGuiKey_Space] = M(SDLK_SPACE);
    io.KeyMap[ImGuiKey_Enter] = M(SDLK_RETURN);
    io.KeyMap[ImGuiKey_Escape] = M(SDLK_ESCAPE);
    io.KeyMap[ImGuiKey_A] = SDLK_A;
    io.KeyMap[ImGuiKey_C] = SDLK_C;
    io.KeyMap[ImGuiKey_V] = SDLK_V;
    io.KeyMap[ImGuiKey_X] = SDLK_X;
    io.KeyMap[ImGuiKey_Y] = SDLK_Y;
    io.KeyMap[ImGuiKey_Z] = SDLK_Z;
#undef M

    io.SetClipboardTextFn = ImGui_ImplSdlGL3_SetClipboardText;
    io.GetClipboardTextFn = ImGui_ImplSdlGL3_GetClipboardText;
    io.ClipboardUserData = NULL;

    g_MouseCursors[ImGuiMouseCursor_Arrow] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_DEFAULT);
    g_MouseCursors[ImGuiMouseCursor_TextInput] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_TEXT);
    g_MouseCursors[ImGuiMouseCursor_ResizeAll] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_MOVE);
    g_MouseCursors[ImGuiMouseCursor_ResizeNS] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_NS_RESIZE);
    g_MouseCursors[ImGuiMouseCursor_ResizeEW] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_EW_RESIZE);
    g_MouseCursors[ImGuiMouseCursor_ResizeNESW] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_NESW_RESIZE);
    g_MouseCursors[ImGuiMouseCursor_ResizeNWSE] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_NWSE_RESIZE);

#ifdef _WIN32
    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);
    SDL_GetWindowWMInfo(window, &wmInfo);
    io.ImeWindowHandle = wmInfo.info.win.window;
#else
    (void)window;
#endif

    int w, h;
    int display_w, display_h;
    SDL_GetWindowSize(window, &w, &h);
    SDL_GetWindowSizeInPixels(window, &display_w, &display_h);
    io.DisplaySize = ImVec2((float)w, (float)h);
    io.DisplayFramebufferScale = ImVec2(w > 0 ? ((float)display_w / w) : 0, h > 0 ? ((float)display_h / h) : 0);
    return true;
}

void ImGui_ImplSdlGL3_Shutdown()
{
    // Destroy SDL mouse cursors
    for (ImGuiMouseCursor cursor_n = 0; cursor_n < ImGuiMouseCursor_COUNT; cursor_n++)
        SDL_DestroyCursor(g_MouseCursors[cursor_n]);
    memset(g_MouseCursors, 0, sizeof(g_MouseCursors));

    // Destroy OpenGL objects
    ImGui_ImplSdlGL3_InvalidateDeviceObjects();
}

void ImGui_ImplSdlGL3_NewFrame(SDL_Window* window)
{
    if (!g_FontTexture)
        ImGui_ImplSdlGL3_CreateDeviceObjects();

    ImGuiIO& io = ImGui::GetIO();

    // Setup display size (every frame to accommodate for window resizing)
    int w, h;
    int display_w, display_h;
    SDL_GetWindowSize(window, &w, &h);
    SDL_GetWindowSizeInPixels(window, &display_w, &display_h);
    io.DisplaySize = ImVec2((float)w, (float)h);
    io.DisplayFramebufferScale = ImVec2(w > 0 ? ((float)display_w / w) : 0, h > 0 ? ((float)display_h / h) : 0);

    // Setup time step (we don't use SDL_GetTicks() because it is using millisecond resolution)
    static Uint64 frequency = SDL_GetPerformanceFrequency();
    Uint64 current_time = SDL_GetPerformanceCounter();
    io.DeltaTime = g_Time > 0 ? (float)((double)(current_time - g_Time) / frequency) : (float)(1.0f / 60.0f);
    g_Time = current_time;

    // Setup mouse inputs (we already got mouse wheel, keyboard keys & characters from our event handler)
    float mx, my;
    Uint32 mouse_buttons = SDL_GetMouseState(&mx, &my);
    io.MousePos = ImVec2(-FLT_MAX, -FLT_MAX);
    io.MouseDown[0] = g_MousePressed[0] || (mouse_buttons & SDL_BUTTON_MASK(SDL_BUTTON_LEFT)) != 0;  // If a mouse press event came, always pass it as "mouse held this frame", so we don't miss click-release events that are shorter than 1 frame.
    io.MouseDown[1] = g_MousePressed[1] || (mouse_buttons & SDL_BUTTON_MASK(SDL_BUTTON_RIGHT)) != 0;
    io.MouseDown[2] = g_MousePressed[2] || (mouse_buttons & SDL_BUTTON_MASK(SDL_BUTTON_MIDDLE)) != 0;
    g_MousePressed[0] = g_MousePressed[1] = g_MousePressed[2] = false;

    // We need to use SDL_CaptureMouse() to easily retrieve mouse coordinates outside of the client area. This is only supported from SDL 2.0.4 (released Jan 2016)
#if (SDL_MAJOR_VERSION >= 2) && (SDL_MINOR_VERSION >= 0) && (SDL_PATCHLEVEL >= 4)
    if ((SDL_GetWindowFlags(window) & (SDL_WINDOW_MOUSE_FOCUS | SDL_WINDOW_MOUSE_CAPTURE)) != 0)
        io.MousePos = ImVec2((float)mx, (float)my);
    bool any_mouse_button_down = false;
    for (int n = 0; n < IM_ARRAYSIZE(io.MouseDown); n++)
        any_mouse_button_down |= io.MouseDown[n];
    if (any_mouse_button_down && (SDL_GetWindowFlags(window) & SDL_WINDOW_MOUSE_CAPTURE) == 0)
        SDL_CaptureMouse(SDL_TRUE);
    if (!any_mouse_button_down && (SDL_GetWindowFlags(window) & SDL_WINDOW_MOUSE_CAPTURE) != 0)
        SDL_CaptureMouse(SDL_FALSE);
#else
    if ((SDL_GetWindowFlags(window) & SDL_WINDOW_INPUT_FOCUS) != 0)
        io.MousePos = ImVec2((float)mx, (float)my);
#endif

    // Update OS/hardware mouse cursor if imgui isn't drawing a software cursor
    if ((io.ConfigFlags & ImGuiConfigFlags_NoMouseCursorChange) == 0)
    {
        ImGuiMouseCursor cursor = ImGui::GetMouseCursor();
        if (io.MouseDrawCursor || cursor == ImGuiMouseCursor_None)
        {
            SDL_HideCursor();
        }
        else
        {
            SDL_SetCursor(g_MouseCursors[cursor] ? g_MouseCursors[cursor] : g_MouseCursors[ImGuiMouseCursor_Arrow]);
            SDL_ShowCursor();
        }
    }

    // Start the frame. This call will update the io.WantCaptureMouse, io.WantCaptureKeyboard flag that you can use to dispatch inputs (or not) to your application.
    ImGui::NewFrame();

    if (io.WantCaptureKeyboard) {
         SDL_StartTextInput(window);
    } else {
         SDL_StopTextInput(window);
    }
}
