#include <cassert>
#include <memory>
#include <string>

#include <imgui.h>
#include <kaguya/kaguya.hpp>
#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui_internal.h>

#include "Colormap.hpp"
#include "Image.hpp"
#include "ImageCollection.hpp"
#include "Player.hpp"
#include "SVG.hpp"
#include "Sequence.hpp"
#include "Terminal.hpp"
#include "View.hpp"
#include "Window.hpp"
#include "config.hpp"
#include "events.hpp"
#include "globals.hpp"
#include "lua.hpp"
#include "luafiles.hpp"

static std::unique_ptr<lua_State, decltype(lua_close)*> L = { nullptr, lua_close };
static std::unique_ptr<kaguya::State> state = nullptr;

KAGUYA_MEMBER_FUNCTION_OVERLOADS(sequence_set_edit, Sequence, setEdit, 1, 2)
KAGUYA_FUNCTION_OVERLOADS(is_key_pressed, isKeyPressed, 1, 2)

static std::vector<std::shared_ptr<Window>>& getWindows()
{
    return gWindows;
}

static std::vector<std::shared_ptr<Sequence>>& getSequences()
{
    return gSequences;
}

static std::vector<std::shared_ptr<View>>& getViews()
{
    return gViews;
}

static std::vector<std::shared_ptr<Colormap>>& getColormaps()
{
    return gColormaps;
}

static std::vector<std::shared_ptr<Player>>& getPlayers()
{
    return gPlayers;
}

std::shared_ptr<View> newView()
{
    auto view = std::make_shared<View>();
    gViews.push_back(view);
    return view;
}

std::shared_ptr<Player> newPlayer()
{
    auto player = std::make_shared<Player>();
    gPlayers.push_back(player);
    return player;
}

std::shared_ptr<Colormap> newColormap()
{
    auto colormap = std::make_shared<Colormap>();
    gColormaps.push_back(colormap);
    return colormap;
}

std::shared_ptr<Window> newWindow()
{
    auto window = std::make_shared<Window>();
    gWindows.push_back(window);
    return window;
}

std::shared_ptr<Sequence> newSequence(std::shared_ptr<Colormap> c, std::shared_ptr<Player> p, std::shared_ptr<View> v)
{
    auto seq = std::make_shared<Sequence>();
    gSequences.push_back(seq);
    seq->attachView(std::move(v));
    seq->attachPlayer(std::move(p));
    seq->attachColormap(std::move(c));
    return seq;
}

static void beginWindow(const std::string& name)
{
    bool opened = true;
    ImGui::Begin(name.c_str(), &opened);
}

static void endWindow()
{
    ImGui::End();
}

static bool button(const std::string& name)
{
    return ImGui::Button(name.c_str());
}

static void text(const std::string& text)
{
    return ImGui::Text("%s", text.c_str());
}

static void sameline()
{
    ImGui::SameLine();
}

static void reload()
{
    ImageCache::flush();
    ImageCache::Error::flush();
    for (const auto& seq : gSequences) {
        seq->forgetImage();
    }
    gActive = std::max(gActive, 2);
}

static void reload_svgs()
{
    SVG::flushCache();
    gActive = std::max(gActive, 2);
}

static void settheme(const ImGuiStyle& theme)
{
    ImGui::GetStyle() = theme;
    gActive = std::max(gActive, 2);
}

static const std::string& getTerminalCommand()
{
    return gTerminal.bufcommand;
}

static void setTerminalCommand(const std::string& cmd)
{
    gTerminal.bufcommand = cmd;
    gTerminal.setVisible(true);
    gTerminal.focusInput = false;
}

static std::vector<std::vector<float>> image_get_pixels_from_coords(const Image& img, std::vector<size_t> xs, std::vector<size_t> ys)
{
    std::vector<std::vector<float>> ret;
    if (xs.size() != ys.size()) {
        throw std::runtime_error("inconsistent size between xs and ys");
    }
    for (size_t i = 0; i < xs.size(); i++) {
        size_t x = xs[i];
        size_t y = ys[i];
        std::vector<float> values(img.c);
        img.getPixelValueAt(x, y, values.data(), img.c);
        ret.push_back(std::move(values));
    }
    return ret;
}

static void sequence_set_glob(Sequence* seq, const std::string& glob)
{
    auto filenames = buildFilenamesFromExpression(glob);
    auto col = buildImageCollectionFromFilenames(filenames);
    seq->setImageCollection(col, glob);
}

bool selection_is_shown()
{
    return !gSelecting && gSelectionShown;
}

void config::load()
{
    L = std::unique_ptr<lua_State, decltype(lua_close)*>({ luaL_newstate(), lua_close });
    assert(L);
    luaL_openlibs(L.get());

    state = std::make_unique<kaguya::State>(L.get());

    (*state)["iskeydown"] = isKeyDown;
    (*state)["iskeypressed"] = kaguya::function(is_key_pressed());
    (*state)["iskeyreleased"] = isKeyReleased;
    (*state)["ismouseclicked"] = ImGui::IsMouseClicked;
    (*state)["ismousedown"] = ImGui::IsMouseDown;
    (*state)["ismousereleased"] = ImGui::IsMouseReleased;
    (*state)["getmouseposition"] = ImGui::GetMousePos;
    (*state)["reload"] = reload;
    (*state)["reload_svgs"] = reload_svgs;
    (*state)["set_theme"] = settheme;

    (*state)["begin_window"] = beginWindow;
    (*state)["end_window"] = endWindow;
    (*state)["button"] = button;
    (*state)["text"] = text;
    (*state)["sameline"] = sameline;

    (*state)["ImVec2"].setClass(kaguya::UserdataMetatable<ImVec2>()
                                    .setConstructors<ImVec2(), ImVec2(float, float)>()
                                    .addProperty("x", &ImVec2::x)
                                    .addProperty("y", &ImVec2::y)
                                    .addStaticFunction("__add", [](const ImVec2& a, const ImVec2& b) { return a + b; })
                                    .addStaticFunction("__sub", [](const ImVec2& a, const ImVec2& b) { return a - b; })
                                    .addStaticOverloadedFunctions(
                                        "__div",
                                        [](const ImVec2& a, const ImVec2& b) { return a / b; },
                                        [](const ImVec2& a, const float& b) { return a / b; }));

    (*state)["ImVec4"].setClass(kaguya::UserdataMetatable<ImVec4>()
                                    .setConstructors<ImVec4(), ImVec4(float, float, float, float)>()
                                    .addProperty("x", &ImVec4::x)
                                    .addProperty("y", &ImVec4::y)
                                    .addProperty("z", &ImVec4::z)
                                    .addProperty("w", &ImVec4::w));

    (*state)["ImRect"].setClass(kaguya::UserdataMetatable<ImRect>()
                                    .setConstructors<ImRect(), ImRect(ImVec2, ImVec2)>()
                                    .addProperty("min", &ImRect::Min)
                                    .addProperty("max", &ImRect::Max)
                                    .addFunction("get_width", &ImRect::GetWidth)
                                    .addFunction("get_height", &ImRect::GetHeight)
                                    .addFunction("get_size", &ImRect::GetSize)
                                    .addFunction("get_center", &ImRect::GetCenter));

    (*state)["ImGuiStyle"].setClass(kaguya::UserdataMetatable<ImGuiStyle>()
                                        .setConstructors<ImGuiStyle()>()
                                        .addProperty("Alpha", &ImGuiStyle::Alpha)
                                        .addProperty("WindowPadding", &ImGuiStyle::WindowPadding)
                                        .addProperty("WindowRounding", &ImGuiStyle::WindowRounding)
                                        .addProperty("WindowBorderSize", &ImGuiStyle::WindowBorderSize)
                                        .addProperty("WindowMinSize", &ImGuiStyle::WindowMinSize)
                                        .addProperty("WindowTitleAlign", &ImGuiStyle::WindowTitleAlign)
                                        .addProperty("ChildRounding", &ImGuiStyle::ChildRounding)
                                        .addProperty("ChildBorderSize", &ImGuiStyle::ChildBorderSize)
                                        .addProperty("PopupRounding", &ImGuiStyle::PopupRounding)
                                        .addProperty("PopupBorderSize", &ImGuiStyle::PopupBorderSize)
                                        .addProperty("FramePadding", &ImGuiStyle::FramePadding)
                                        .addProperty("FrameRounding", &ImGuiStyle::FrameRounding)
                                        .addProperty("FrameBorderSize", &ImGuiStyle::FrameBorderSize)
                                        .addProperty("ItemSpacing", &ImGuiStyle::ItemSpacing)
                                        .addProperty("ItemInnerSpacing", &ImGuiStyle::ItemInnerSpacing)
                                        .addProperty("TouchExtraPadding", &ImGuiStyle::TouchExtraPadding)
                                        .addProperty("IndentSpacing", &ImGuiStyle::IndentSpacing)
                                        .addProperty("ColumnsMinSpacing", &ImGuiStyle::ColumnsMinSpacing)
                                        .addProperty("ScrollbarSize", &ImGuiStyle::ScrollbarSize)
                                        .addProperty("ScrollbarRounding", &ImGuiStyle::ScrollbarRounding)
                                        .addProperty("GrabMinSize", &ImGuiStyle::GrabMinSize)
                                        .addProperty("GrabRounding", &ImGuiStyle::GrabRounding)
                                        .addProperty("ButtonTextAlign", &ImGuiStyle::ButtonTextAlign)
                                        .addProperty("DisplayWindowPadding", &ImGuiStyle::DisplayWindowPadding)
                                        .addProperty("DisplaySafeAreaPadding", &ImGuiStyle::DisplaySafeAreaPadding)
                                        .addProperty("MouseCursorScale", &ImGuiStyle::MouseCursorScale)
                                        .addProperty("AntiAliasedLines", &ImGuiStyle::AntiAliasedLines)
                                        .addProperty("AntiAliasedFill", &ImGuiStyle::AntiAliasedFill)
                                        .addProperty("CurveTessellationTol", &ImGuiStyle::CurveTessellationTol)
                                        .addProperty("Colors", &ImGuiStyle::Colors));

    (*state)["ImGuiCol_Text"] = ImGuiCol_Text + 1;
    (*state)["ImGuiCol_TextDisabled"] = ImGuiCol_TextDisabled + 1;
    (*state)["ImGuiCol_WindowBg"] = ImGuiCol_WindowBg + 1;
    (*state)["ImGuiCol_ChildBg"] = ImGuiCol_ChildBg + 1;
    (*state)["ImGuiCol_PopupBg"] = ImGuiCol_PopupBg + 1;
    (*state)["ImGuiCol_Border"] = ImGuiCol_Border + 1;
    (*state)["ImGuiCol_BorderShadow"] = ImGuiCol_BorderShadow + 1;
    (*state)["ImGuiCol_FrameBg"] = ImGuiCol_FrameBg + 1;
    (*state)["ImGuiCol_FrameBgHovered"] = ImGuiCol_FrameBgHovered + 1;
    (*state)["ImGuiCol_FrameBgActive"] = ImGuiCol_FrameBgActive + 1;
    (*state)["ImGuiCol_TitleBg"] = ImGuiCol_TitleBg + 1;
    (*state)["ImGuiCol_TitleBgActive"] = ImGuiCol_TitleBgActive + 1;
    (*state)["ImGuiCol_TitleBgCollapsed"] = ImGuiCol_TitleBgCollapsed + 1;
    (*state)["ImGuiCol_MenuBarBg"] = ImGuiCol_MenuBarBg + 1;
    (*state)["ImGuiCol_ScrollbarBg"] = ImGuiCol_ScrollbarBg + 1;
    (*state)["ImGuiCol_ScrollbarGrab"] = ImGuiCol_ScrollbarGrab + 1;
    (*state)["ImGuiCol_ScrollbarGrabHovered"] = ImGuiCol_ScrollbarGrabHovered + 1;
    (*state)["ImGuiCol_ScrollbarGrabActive"] = ImGuiCol_ScrollbarGrabActive + 1;
    (*state)["ImGuiCol_CheckMark"] = ImGuiCol_CheckMark + 1;
    (*state)["ImGuiCol_SliderGrab"] = ImGuiCol_SliderGrab + 1;
    (*state)["ImGuiCol_SliderGrabActive"] = ImGuiCol_SliderGrabActive + 1;
    (*state)["ImGuiCol_Button"] = ImGuiCol_Button + 1;
    (*state)["ImGuiCol_ButtonHovered"] = ImGuiCol_ButtonHovered + 1;
    (*state)["ImGuiCol_ButtonActive"] = ImGuiCol_ButtonActive + 1;
    (*state)["ImGuiCol_Header"] = ImGuiCol_Header + 1;
    (*state)["ImGuiCol_HeaderHovered"] = ImGuiCol_HeaderHovered + 1;
    (*state)["ImGuiCol_HeaderActive"] = ImGuiCol_HeaderActive + 1;
    (*state)["ImGuiCol_Separator"] = ImGuiCol_Separator + 1;
    (*state)["ImGuiCol_SeparatorHovered"] = ImGuiCol_SeparatorHovered + 1;
    (*state)["ImGuiCol_SeparatorActive"] = ImGuiCol_SeparatorActive + 1;
    (*state)["ImGuiCol_ResizeGrip"] = ImGuiCol_ResizeGrip + 1;
    (*state)["ImGuiCol_ResizeGripHovered"] = ImGuiCol_ResizeGripHovered + 1;
    (*state)["ImGuiCol_ResizeGripActive"] = ImGuiCol_ResizeGripActive + 1;
    (*state)["ImGuiCol_PlotLines"] = ImGuiCol_PlotLines + 1;
    (*state)["ImGuiCol_PlotLinesHovered"] = ImGuiCol_PlotLinesHovered + 1;
    (*state)["ImGuiCol_PlotHistogram"] = ImGuiCol_PlotHistogram + 1;
    (*state)["ImGuiCol_PlotHistogramHovered"] = ImGuiCol_PlotHistogramHovered + 1;
    (*state)["ImGuiCol_TextSelectedBg"] = ImGuiCol_TextSelectedBg + 1;
    (*state)["ImGuiCol_ModalWindowDarkening"] = ImGuiCol_ModalWindowDarkening + 1;
    (*state)["ImGuiCol_DragDropTarget"] = ImGuiCol_DragDropTarget + 1;
    (*state)["ImGuiCol_NavHighlight"] = ImGuiCol_NavHighlight + 1;
    (*state)["ImGuiCol_NavWindowingHighlight"] = ImGuiCol_NavWindowingHighlight + 1;

    (*state)["PLAMBDA"] = PLAMBDA;
    (*state)["OCTAVE"] = OCTAVE;

    (*state)["Player"].setClass(kaguya::UserdataMetatable<Player>()
                                    .addProperty("id", &Player::ID)
                                    .addProperty("opened", &Player::opened)
                                    .addProperty("frame", &Player::frame)
                                    .addProperty("playing", &Player::playing)
                                    .addProperty("fps", &Player::fps)
                                    .addProperty("looping", &Player::looping)
                                    .addProperty("bouncy", &Player::bouncy)
                                    .addProperty("current_min_frame", &Player::currentMinFrame)
                                    .addProperty("current_max_frame", &Player::currentMaxFrame)
                                    .addProperty("min_frame", &Player::minFrame)
                                    .addProperty("max_frame", &Player::maxFrame)
                                    .addFunction("check_bounds", &Player::checkBounds));

    (*state)["View"].setClass(kaguya::UserdataMetatable<View>()
                                  .addProperty("id", &View::ID)
                                  .addProperty("center", &View::center)
                                  .addProperty("zoom", &View::zoom)
                                  .addProperty("should_rescale", &View::shouldRescale)
                                  .addProperty("svg_offset", &View::svgOffset));

    (*state)["Colormap"].setClass(kaguya::UserdataMetatable<Colormap>()
                                      .addProperty("id", &Colormap::ID)
                                      .addFunction("set_shader", &Colormap::setShader)
                                      .addFunction("get_shader", &Colormap::getShaderName)
                                      .addStaticFunction("get_range", [](Colormap* c, int n) {
                                          float min, max;
                                          c->getRange(min, max, n);
                                          return std::tuple<float, float>(min, max);
                                      })
                                      .addProperty("bands", &Colormap::bands)
                                      .addProperty("center", &Colormap::center)
                                      .addProperty("radius", &Colormap::radius));

    (*state)["Image"].setClass(kaguya::UserdataMetatable<Image>()
                                   .addProperty("id", &Image::ID)
                                   .addProperty("channels", &Image::c)
                                   .addProperty("size", &Image::size));
    (*state)["image_get_pixels_from_coords"] = image_get_pixels_from_coords;
    (*state)["get_image_by_id"] = ImageCache::getById;

    (*state)["ImageCollection"].setClass(kaguya::UserdataMetatable<ImageCollection>()
                                             .addFunction("get_filename", &ImageCollection::getFilename)
                                             .addFunction("get_length", &ImageCollection::getLength));

    (*state)["Sequence"].setClass(kaguya::UserdataMetatable<Sequence>()
                                      .addProperty("id", &Sequence::ID)
                                      .addProperty("player", &Sequence::player)
                                      .addProperty("view", &Sequence::view)
                                      .addProperty("colormap", &Sequence::colormap)
                                      .addProperty("image", &Sequence::image)
                                      .addProperty("collection", &Sequence::collection)
                                      .addStaticFunction("set_glob", sequence_set_glob)
                                      .addFunction("set_edit", sequence_set_edit())
                                      .addFunction("get_edit", &Sequence::getEdit)
                                      .addFunction("get_id", &Sequence::getId)
                                      .addFunction("put_script_svg", &Sequence::putScriptSVG));

    (*state)["Window"].setClass(kaguya::UserdataMetatable<Window>()
                                    .addProperty("id", &Window::ID)
                                    .addProperty("opened", &Window::opened)
                                    .addProperty("position", &Window::position)
                                    .addProperty("size", &Window::size)
                                    .addProperty("force_geometry", &Window::forceGeometry)
                                    .addProperty("content_rect", &Window::contentRect)
                                    .addProperty("index", &Window::index)
                                    .addProperty("sequences", &Window::sequences)
                                    .addProperty("dont_layout", &Window::dontLayout)
                                    .addProperty("always_on_top", &Window::alwaysOnTop)
                                    .addProperty("screenshot", &Window::screenshot));

    (*state)["gHoveredPixel"] = &gHoveredPixel;
    (*state)["selection_is_shown"] = selection_is_shown;
    (*state)["gSelectionFrom"] = &gSelectionFrom;
    (*state)["gSelectionTo"] = &gSelectionTo;
    (*state)["get_windows"] = getWindows;
    (*state)["get_sequences"] = getSequences;
    (*state)["get_views"] = getViews;
    (*state)["get_colormaps"] = getColormaps;
    (*state)["get_players"] = getPlayers;
    (*state)["new_sequence"] = newSequence;
    (*state)["new_window"] = newWindow;
    (*state)["new_view"] = newView;
    (*state)["new_player"] = newPlayer;
    (*state)["new_colormap"] = newColormap;
    (*state)["get_terminal_command"] = getTerminalCommand;
    (*state)["set_terminal_command"] = setTerminalCommand;

    (*state)["GL3"] = true;

    load_luafiles(L.get());
}

float config::get_float(const std::string& name)
{
    return (*state)[name.c_str()];
}

bool config::get_bool(const std::string& name)
{
    return (*state)[name.c_str()];
}

int config::get_int(const std::string& name)
{
    auto v = (*state)[name.c_str()];
    if (v.type() == LUA_TBOOLEAN) {
        return (bool)v;
    }
    return v;
}

std::string config::get_string(const std::string& name)
{
    return (*state)[name.c_str()];
}

#include "shaders.hpp"
void config::load_shaders()
{
    std::map<std::string, std::string> shaders = (*state)["SHADERS"];
    for (const auto& s : shaders) {
        loadShader(s.first, s.second);
    }
}

kaguya::State& config::get_lua()
{
    return *state;
}
