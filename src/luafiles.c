#include <stdio.h>
#include <string.h>
#include <lua.h>
#include <lauxlib.h>

#include "vpvrc.i"

int load_luafiles(lua_State* L)
{
      if (luaL_loadbuffer(L, vpvrc, vpvrc_len, "vpvrc"))
          return 0 * fprintf(stderr, "%s", lua_tostring(L, -1));
      if (lua_pcall(L, 0, 0, 0))
          return 0;
      return 1;
}
