#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lua.h"
#include "lauxlib.h"

#include "skylua.h"

// Convert all arguments to strings and join them with tabs, then send the
// result to the REPL terminal (during a REPL snippet) or the HT console.
static i32 sleLuaPrint(lua_State *L) {
  int count = lua_gettop(L);
  int i;
  size_t total = 1;
  char *buffer;
  char *cursor;

  // Measure the converted arguments first.
  for (i = 1; i <= count; ++i) {
    size_t len = 0;
    luaL_tolstring(L, i, &len);
    total += len + 1;
    lua_pop(L, 1);
  }

  buffer = (char *)malloc(total);
  if (!buffer)
    return 0;

  cursor = buffer;

  for (i = 1; i <= count; ++i) {
    size_t len = 0;
    const char *text = luaL_tolstring(L, i, &len);

    if (i > 1)
      *cursor++ = '\t';
    if (text && len) {
      memcpy(cursor, text, len);
      cursor += len;
    }
    lua_pop(L, 1);
  }

  *cursor = 0;
  sleLogText(buffer);
  free(buffer);

  return 0;
}

// Add <ModFolder>/scripts to package.path so that require() can load modules
// from the scripts folder, for example require("lib.util") loads
// <ModFolder>/scripts/lib/util.lua.
static void addScriptsToPackagePath(lua_State *L) {
  const char *root = sleGetScriptsPathUtf8();
  const char *current;
  char *path;
  size_t needed;

  if (!root || !root[0])
    return;

  lua_getglobal(L, "package");
  if (!lua_istable(L, -1)) {
    lua_pop(L, 1);
    return;
  }

  lua_getfield(L, -1, "path");
  current = lua_tostring(L, -1);

  needed = (size_t)strlen(root) * 2 + 32;
  if (current)
    needed += strlen(current);

  path = (char *)malloc(needed);
  if (!path) {
    lua_pop(L, 2);
    return;
  }

  // Append the scripts folder without overriding modules provided by the
  // game. '?' is replaced by the module name by the standard Lua loader and
  // '?' followed by /init.lua supports package-style modules.
  snprintf(
    path, needed,
    "%s%s%s\\?.lua;%s\\?\\init.lua",
    current && current[0] ? current : "",
    current && current[0] ? ";" : "",
    root, root);

  lua_pop(L, 1);
  lua_pushstring(L, path);
  lua_setfield(L, -2, "path");
  lua_pop(L, 1);

  free(path);
}

i32 sleAddLuaBindings(lua_State *L) {
  // Register the namespace: _G.sle = {}
  lua_newtable(L);
  lua_setglobal(L, "sle");

  lua_getglobal(L, "sle");

  lua_pushcfunction(L, sleLuaPrint);
  lua_setfield(L, -2, "log");

  // sle.print() mirrors print() but always respects the REPL capture state.
  lua_pushcfunction(L, sleLuaPrint);
  lua_setfield(L, -2, "print");

  lua_pop(L, 1);

  // Make scripts in <ModFolder>/scripts available to require().
  addScriptsToPackagePath(L);

  return 1;
}
