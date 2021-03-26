# put .lua files into a single .cpp file
# the function load_luafiles runs those codes and return 1 if success
set(LUAFILES_LIST "vpvrc")

set(LUAFILES_OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/luafiles.c)

# generate luafiles.c
file(WRITE ${LUAFILES_OUTPUT} "
#include <stdio.h>
#include <string.h>
#include <lua.h>
#include <lauxlib.h>

int load_luafiles(lua_State* L)
{
    (void) L;")
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
        const char* file = \"${luacode_short}\";
        const char* code = \"${FILE_CONTENT}\";
        if (luaL_loadbuffer(L, code, strlen(code), file)) {
            fprintf(stderr, \"%s\", lua_tostring(L, -1));
            return 0;
        }
        if (lua_pcall(L, 0, 0, 0)) {
            fprintf(stderr, \"%s\\n\", lua_tostring(L, -1));
            return 0;
        }
    }
")
endforeach (luacode)
file(APPEND ${LUAFILES_OUTPUT} "
    return 1;
}")

list(APPEND SOURCES ${LUAFILES_OUTPUT})

