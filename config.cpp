#include <string>
#include <cassert>

#include "lua.hpp"
#include "config.hpp"

#define S(...) #__VA_ARGS__

static const char* defaults = S(
SCALE = 1
WATCH = false
CACHE = true
SCREENSHOT = 'screenshot_%d.png'

WINDOW_WIDTH = 1024
WINDOW_HEIGHT = 720

SHOW_HUD = true
SHOW_SVG = true
SHOW_MENUBAR = true

DEFAULT_LAYOUT = "grid"
AUTOZOOM = true
SATURATION = 0.05

-- load user config \n
f = loadfile(os.getenv('HOME') .. '/.vpvrc')
if f then f() end
-- load folder config \n
f = loadfile('.vpvrc')
if f then f() end

-- for compatibility.. remove me one day \n
if os.getenv('SCALE') then SCALE = tonumber(os.getenv('SCALE')) end
if os.getenv('WATCH') then WATCH = tonumber(os.getenv('WATCH')) end
if os.getenv('CACHE') then CACHE = tonumber(os.getenv('CACHE')) end
if WATCH == 0 then WATCH = false end
if CACHE == 0 then CACHE = false end
);

static lua_State* L;

static int report (lua_State *L, int status) {
    const char *msg;
    if (status) {
        msg = lua_tostring(L, -1);
        if (msg == NULL) msg = "(error with no message)";
        fprintf(stderr, "status=%d, %s\n", status, msg);
        lua_pop(L, 1);
    }
    return status;
}

void config::load()
{
    L = luaL_newstate();
    assert(L);
    luaL_openlibs(L);
    report(L, luaL_loadstring(L, defaults) || lua_pcall(L, 0, 0, 0));
}

float config::get_float(const std::string& name)
{
    lua_getglobal(L, name.c_str());
    float num = luaL_checknumber(L, -1);
    lua_pop(L, 1);
    return num;
}

bool config::get_bool(const std::string& name)
{
    lua_getglobal(L, name.c_str());
    float num = lua_toboolean(L, -1);
    lua_pop(L, 1);
    return num;
}

std::string config::get_string(const std::string& name)
{
    lua_getglobal(L, name.c_str());
    const char* str = luaL_checkstring(L, -1);
    std::string ret(str);
    lua_pop(L, 1);
    return ret;
}

