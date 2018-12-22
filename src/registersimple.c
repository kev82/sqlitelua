#include <sqlite3ext.h>
SQLITE_EXTENSION_INIT3

#include <lua.h>
#include <lauxlib.h>
#include <assert.h>

#include "rclua.h"

typedef struct
{
  rc_lua_state *rcs;
  int funcidx;
} def_simple;

static void execute(sqlite3_context *ctx, int argc, sqlite3_value **argv)
{
  def_simple *def = (def_simple *)sqlite3_user_data(ctx);
  assert(def != NULL);

  coro_state cs;
  rc_lua_initcoro(&cs);
  rc_lua_obtaincoro(def->rcs, &cs);

  lua_rawgeti(cs.coro, LUA_REGISTRYINDEX, def->funcidx);
  for(int i=0;i!=argc;++i)
  {
    switch(sqlite3_value_type(argv[i]))
    {
      case SQLITE_INTEGER:
        lua_pushinteger(cs.coro, sqlite3_value_int64(argv[i]));
        break;
      case SQLITE_FLOAT:
        lua_pushnumber(cs.coro, sqlite3_value_double(argv[i]));
        break;
      case SQLITE_TEXT:
        if(rc_lua_pushstring(cs.coro, sqlite3_value_text(argv[i])) != LUA_OK)
        {
          sqlite3_result_error(ctx, "lua string marsalling failed", -1);
          goto cleanup;
        }
        break;
      case SQLITE_NULL:
        lua_pushnil(cs.coro);
        break;
      default:
        sqlite3_result_error(ctx, "Incompatible argument type", -1);
        goto cleanup;
    }
  }

  int lst = lua_resume(cs.coro, NULL, argc);
  if(lst != LUA_OK)
  {
    sqlite3_result_error(ctx, "simple function yielded or errored", -1);
    goto cleanup;
  }
  lua_settop(cs.coro, 1);

  switch(lua_type(cs.coro, -1))
  {
    case LUA_TNIL:
      sqlite3_result_null(ctx);
      break;
    case LUA_TNUMBER:
      sqlite3_result_double(ctx, lua_tonumber(cs.coro, -1));
      break;
    case LUA_TSTRING:
      sqlite3_result_text(ctx, lua_tostring(cs.coro, -1), -1, SQLITE_TRANSIENT);
      break;
    case LUA_TBOOLEAN:
      sqlite3_result_int(ctx, lua_toboolean(cs.coro, -1));
      break;
    default:
      sqlite3_result_error(ctx, "Unsupported return type", -1);
      break;
  }

cleanup:
  rc_lua_releasecoro(def->rcs, &cs);
}

void freesimpledef(void *v)
{
  def_simple *def = (def_simple *)v;
  rc_lua_release(&def->rcs);
  sqlite3_free(v);
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

  def->rcs = (rc_lua_state *)lua_touserdata(l, lua_upvalueindex(1));
  rc_lua_get(&def->rcs);
  def->funcidx = luaL_ref(l, LUA_REGISTRYINDEX);

  int nargs = (int)lua_tonumber(l, 2);
  const char *name = lua_tostring(l, 1);
  sqlite3 *db = lua_touserdata(l, lua_upvalueindex(2));

  sqlite3_create_function_v2(db, name, nargs, SQLITE_ANY, def, execute, NULL, NULL, freesimpledef);

  return 0;
}

