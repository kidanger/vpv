
cat <<EOF
#include <stdio.h>
#include <string.h>
#include <lua.h>
#include <lauxlib.h>

int load_luafiles(lua_State* L)
{
    (void) L;
EOF

for f in $@; do
  FILE_CONTENT=`cat $f \
    | sed 's/"/\\\"/g' \
    | sed ':a;N;$!ba;s/\n/\\\n/g'
  `
  cat <<EOF
  {
      const char* file = "$f";
      const char* code = "${FILE_CONTENT}";
      if (luaL_loadbuffer(L, code, strlen(code), file)) {
          fprintf(stderr, "%s", lua_tostring(L, -1));
          return 0;
        }
      if (lua_pcall(L, 0, 0, 0)) {
          return 0;
        }
      }
EOF
done

cat <<EOF
    return 1;
}
EOF

