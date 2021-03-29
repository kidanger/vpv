# put .lua files into a single .cpp file
# the function load_luafiles runs those codes and return 1 if success
set(LUAFILES_LIST "vpvrc")

set(LUAFILES_OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/luafiles.cpp)

# generate luafiles.cpp
file(WRITE ${LUAFILES_OUTPUT} "
#include <iostream>
#include <string>
#include <map>

#include <lua.hpp>

int load_luafiles(lua_State* L);

int load_luafiles(lua_State* L)
{
    static const std::map<std::string, std::string> luafiles = {
")
foreach (luacode ${LUAFILES_LIST})
    file(READ ${luacode} FILE_CONTENT)
    get_filename_component(luacode_short ${luacode} NAME)
    # \\ => \\\\
    string(REGEX REPLACE "\\\\\\\\" "\\\\\\\\\\\\\\\\" FILE_CONTENT "${FILE_CONTENT}")
    # \n => \\n
    string(REGEX REPLACE "\\\\\\n" "\\\\\\\\n" FILE_CONTENT "${FILE_CONTENT}")
    # " => \"
    string(REGEX REPLACE "\\\"" "\\\\\"" FILE_CONTENT "${FILE_CONTENT}")
    # <eol> => \n\<eol>
    string(REGEX REPLACE "\n" "\\\\n\\\\\n" FILE_CONTENT "${FILE_CONTENT}")
    file(APPEND ${LUAFILES_OUTPUT} "
    {
        \"${luacode_short}\",
        \"${FILE_CONTENT}\"
    }
")
endforeach (luacode)
file(APPEND ${LUAFILES_OUTPUT} "
    };
    for (const auto &kv : luafiles) {
        const auto &file = kv.first;
        const auto &code = kv.second;
        if (::luaL_loadbuffer(L, code.c_str(), code.size(), file.c_str())) {
            std::cerr << lua_tostring(L, -1) << std::endl;
            return 0;
        }
        if (::lua_pcall(L, 0, 0, 0)) {
            std::cerr << lua_tostring(L, -1) << std::endl;
            return 0;
        }
    }
    return 1;
}
")

list(APPEND SOURCES ${LUAFILES_OUTPUT})

