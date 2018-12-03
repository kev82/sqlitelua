#include <sqlite3ext.h>
SQLITE_EXTENSION_INIT3

#include <lua.h>
#include <lauxlib.h>
#include <assert.h>

typedef struct
{
  lua_State *main;
  int funcidx;
  int aggflagidx;
} def_aggregate;

typedef struct
{
  lua_State *coro;
  int idx;
} agg_context;

static void execute_step(sqlite3_context *ctx, int argc, sqlite3_value **argv)
{
  def_aggregate *def = (def_aggregate *)sqlite3_user_data(ctx);

  agg_context *agg = (agg_context *)sqlite3_aggregate_context(ctx, sizeof(agg_context));
  assert(agg != NULL);
  if(agg->coro == 0)
  {
    agg->coro = lua_newthread(def->main);
    agg->idx = luaL_ref(def->main, LUA_REGISTRYINDEX);
    lua_rawgeti(agg->coro, LUA_REGISTRYINDEX, def->funcidx);
  }
  else
  {
    assert(lua_status(agg->coro) == LUA_YIELD);
  }

  lua_State *l = agg->coro;

  for(int i=0;i!=argc;++i)
  {
    switch(sqlite3_value_type(argv[i]))
    {
      case SQLITE_INTEGER:
        lua_pushinteger(l, sqlite3_value_int64(argv[i]));
        break;
      case SQLITE_FLOAT:
        lua_pushnumber(l, sqlite3_value_double(argv[i]));
        break;
      case SQLITE_TEXT:
        lua_pushstring(l, sqlite3_value_text(argv[i]));
        break;
      case SQLITE_NULL:
        lua_pushnil(l);
        break;
      default:
        sqlite3_result_error(ctx, "Incompatible argument type", -1);
        goto cleanup;
    }
  }

  if(lua_resume(l, NULL, argc) != LUA_YIELD)
  {
    sqlite3_result_error(ctx, "Error aggregating", -1);
    goto cleanup;
  }
  lua_settop(l, 0);

  return;
cleanup:
  lua_pushnil(def->main);
  lua_rawseti(def->main, LUA_REGISTRYINDEX, agg->idx);
  agg->coro = 0;
}

static void execute_final(sqlite3_context *ctx)
{
  def_aggregate *def = (def_aggregate *)sqlite3_user_data(ctx);

  agg_context *agg = (agg_context *)sqlite3_aggregate_context(ctx, sizeof(agg_context));
  assert(agg != NULL);
  if(agg->coro == 0)
  {
    agg->coro = lua_newthread(def->main);
    agg->idx = luaL_ref(def->main, LUA_REGISTRYINDEX);
    lua_rawgeti(agg->coro, LUA_REGISTRYINDEX, def->funcidx);
  }
  else
  {
    assert(lua_status(agg->coro) == LUA_YIELD);
  }

  lua_State *l = agg->coro;
  lua_rawgeti(l, LUA_REGISTRYINDEX, def->aggflagidx);
  switch(lua_resume(l, NULL, 1))
  {
    case LUA_OK:
      break;
    case LUA_YIELD:
      sqlite3_result_error(ctx, "Aggregator didn't finish", -1);
      goto cleanup;
    case LUA_ERRRUN:
      assert(lua_gettop(l) >= 1);
      sqlite3_result_error(ctx, lua_tolstring(l, -1, NULL), -1);
      goto cleanup;
    default:
      sqlite3_result_error(ctx, "Unknown Aggregator error", -1);
      goto cleanup;
  }
  lua_settop(l, 1);

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
    case LUA_TBOOLEAN:
      sqlite3_result_int(ctx, lua_toboolean(l, -1));
      break;
    default:
      sqlite3_result_error(ctx, "Unsupported return type", -1);
      break;
  }

cleanup:
  lua_pushnil(def->main);
  lua_rawseti(def->main, LUA_REGISTRYINDEX, agg->idx);
  assert(lua_gettop(def->main) == 0);
}

int register_aggregate(lua_State *l)
{
  //name, nargs, func
  lua_settop(l, 4);
  luaL_checktype(l, 1, LUA_TSTRING);
  luaL_checktype(l, 2, LUA_TNUMBER);
  luaL_checktype(l, 3, LUA_TFUNCTION);
  luaL_checktype(l, 4, LUA_TTABLE);

  def_aggregate *def = (def_aggregate *)sqlite3_malloc(sizeof(def_aggregate));
  assert(def != NULL);

  def->main = (lua_State *)lua_touserdata(l, lua_upvalueindex(1));
  def->aggflagidx = luaL_ref(l, LUA_REGISTRYINDEX);
  def->funcidx = luaL_ref(l, LUA_REGISTRYINDEX);

  int nargs = (int)lua_tonumber(l, 2);
  const char *name = lua_tostring(l, 1);
  sqlite3 *db = lua_touserdata(l, lua_upvalueindex(2));

  sqlite3_create_function_v2(db, name, nargs, SQLITE_ANY, def, NULL, execute_step, execute_final, sqlite3_free);

  return 0;
}
