#include <sqlite3ext.h>
SQLITE_EXTENSION_INIT3

#include <lua.h>
#include <lauxlib.h>
#include <assert.h>

typedef struct
{
  lua_State *main;
  int funcidx;
} def_simple;

static void execute(sqlite3_context *ctx, int argc, sqlite3_value **argv)
{
  def_simple *def = (def_simple *)sqlite3_user_data(ctx);
  assert(def != NULL);

  assert(lua_gettop(def->main) == 0);
  lua_State *func = lua_newthread(def->main);
  int functhread = luaL_ref(def->main, LUA_REGISTRYINDEX);
  assert(lua_gettop(def->main) == 0);

  lua_rawgeti(func, LUA_REGISTRYINDEX, def->funcidx);
  for(int i=0;i!=argc;++i)
  {
    switch(sqlite3_value_type(argv[i]))
    {
      case SQLITE_INTEGER:
        lua_pushinteger(func, sqlite3_value_int64(argv[i]));
        break;
      case SQLITE_FLOAT:
        lua_pushnumber(func, sqlite3_value_double(argv[i]));
        break;
      case SQLITE_TEXT:
        lua_pushstring(func, sqlite3_value_text(argv[i]));
        break;
      case SQLITE_NULL:
        lua_pushnil(func);
        break;
      default:
        sqlite3_result_error(ctx, "Incompatible argument type", -1);
        goto cleanup;
    }
  }

  int lst = lua_resume(func, NULL, argc);
  if(lst != LUA_OK)
  {
    sqlite3_result_error(ctx, "simple function yielded or errored", -1);
    goto cleanup;
  }
  lua_settop(func, 1);

  switch(lua_type(func, -1))
  {
    case LUA_TNIL:
      sqlite3_result_null(ctx);
      break;
    case LUA_TNUMBER:
      sqlite3_result_double(ctx, lua_tonumber(func, -1));
      break;
    case LUA_TSTRING:
      sqlite3_result_text(ctx, lua_tostring(func, -1), -1, SQLITE_TRANSIENT);
      break;
    case LUA_TBOOLEAN:
      sqlite3_result_int(ctx, lua_toboolean(func, -1));
      break;
    default:
      sqlite3_result_error(ctx, "Unsupported return type", -1);
      break;
  }

cleanup:
  lua_pushnil(def->main);
  lua_rawseti(def->main, LUA_REGISTRYINDEX, functhread);
  assert(lua_gettop(def->main) == 0);
}
  
int register_simplefunc(lua_State *l)
{
  //name, nargs, func
  lua_settop(l, 3);
  luaL_checktype(l, 1, LUA_TSTRING);
  luaL_checktype(l, 2, LUA_TNUMBER);
  luaL_checktype(l, 3, LUA_TFUNCTION);

  def_simple *def = (def_simple *)sqlite3_malloc(sizeof(def_simple));
  assert(def != NULL);

  def->main = (lua_State *)lua_touserdata(l, lua_upvalueindex(1));
  def->funcidx = luaL_ref(l, LUA_REGISTRYINDEX);

  int nargs = (int)lua_tonumber(l, 2);
  const char *name = lua_tostring(l, 1);
  sqlite3 *db = lua_touserdata(l, lua_upvalueindex(2));

  sqlite3_create_function_v2(db, name, nargs, SQLITE_ANY, def, execute, NULL, NULL, sqlite3_free);

  return 0;
}

  

  

  
      
  

    
