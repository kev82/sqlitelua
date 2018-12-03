#include <sqlite3ext.h>
SQLITE_EXTENSION_INIT3

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

static void executelua(sqlite3_context *ctx, int argc, sqlite3_value **argv)
{
  lua_State *l = luaL_newstate();
  luaL_openlibs(l);
  if(luaL_loadstring(l, sqlite3_value_text(argv[0])) != LUA_OK)
  {
    sqlite3_result_error(ctx, "unable to parse code", -1);
    lua_close(l);
    return;
  }

  if(lua_pcall(l, 0, 1, 0) != LUA_OK)
  {
    if(lua_type(l, -1) == LUA_TSTRING)
    {
      sqlite3_result_error(ctx, lua_tostring(l, -1), -1);
    }
    else
    {
      sqlite3_result_error(ctx, "Unexpected error type", -1);
    }
    lua_close(l);
    return;
  }

  switch(lua_type(l, -1))
  {
    case LUA_TNIL:
      sqlite3_result_null(ctx);
      break;
    case LUA_TNUMBER:
      sqlite3_result_double(ctx, lua_tonumber(l, -1));
      break;
    case LUA_TSTRING:
      sqlite3_result_text(ctx, lua_tostring(l, -1), -1, SQLITE_TRANSIENT);
      break;
    default:
      sqlite3_result_error(ctx, "Unsupported return type", -1);
      break;
  }

  lua_close(l);
  return;
}
    
int setup_executelua(sqlite3 *db)
{
  return sqlite3_create_function(db, "executelua", 1, SQLITE_ANY,
   NULL, executelua, NULL, NULL);
}      
