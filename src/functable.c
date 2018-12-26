#include <sqlite3ext.h>
SQLITE_EXTENSION_INIT3

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <assert.h>

#include "rclua.h"

int register_simplefunc(lua_State *l);
int register_aggregate(lua_State *l);
int register_tablefunc(lua_State *l);

typedef struct
{
  sqlite3_vtab vtab;
  rc_lua_state *rcs;
  int insert;
  int iter;
} ft_vtab;

static int setupstate(lua_State *l)
{
  lua_settop(l, 2);
  luaL_checktype(l, 1, LUA_TLIGHTUSERDATA);
  luaL_checktype(l, 2, LUA_TLIGHTUSERDATA);

  lua_pushvalue(l, 1);
  lua_pushvalue(l, 2);
  lua_pushcclosure(l, register_simplefunc, 2);
  lua_setglobal(l, "register_simple");

  lua_pushvalue(l, 1);
  lua_pushvalue(l, 2);
  lua_pushcclosure(l, register_aggregate, 2);
  lua_setglobal(l, "register_aggregate");

  lua_pushvalue(l, 1);
  lua_pushvalue(l, 2);
  lua_pushcclosure(l, register_tablefunc, 2);
  lua_setglobal(l, "register_table");

  lua_settop(l, 0);

  //Returns an inserter and an iterator for our storage table. The iterator is
  //guaranteed to loop over each elemnt once, even if there are insertions mid
  //iteration
  const char *luacode = 
   "local updated = true "
   "local t = {} "
   " "
   "local function newenv() "
   "  local function safecopy(from, to) "
   "    local safetypes = {[ [[function]] ] = true, [ [[string]] ] = true, [ [[number]] ] = true} "
   "    for k, v in pairs(from) do "
   "      if safetypes[type(k)] then "
   "        to[k] = v "
   "      end "
   "    end "
   "  end "
   " "
   "  local rv = {math = {}, coroutine = {}, string = {}, table = {}} "
   " "
   "  safecopy(math, rv.math) "
   "  safecopy(coroutine, rv.coroutine) "
   "  safecopy(string, rv.string) "
   "  safecopy(table, rv.table) "
   " "
   "  rv.assert, rv.error = assert, error "
   "  rv.tostring, rv.tonumber = tostring, tonumber "
   "  rv.pairs, rv.ipairs = pairs, ipairs "
   "  rv.print = print "
   " "
   "  return rv "
   "end "
   " "
   "local function insert(name, src) "
   "  if t[name] then "
   "    error([[function exists]]) "
   "  end "
   " "
   "  local fenv = newenv() "
   "  local code, err = load(src, name, [[t]], fenv) "
   "  if not code then "
   "    error(err) "
   "  end "
   " "
   "  local inputs, outputs, interface, func = code() "
   " "
   "  if interface == [[simple]] then "
   "    register_simple(name, #inputs, func) "
   "  end "
   "  if interface == [[aggregate]] then "
   "    local agid = {} "
   "    function fenv.aggregate_now(x) "
   "      return rawequal(x, agid) "
   "    end "
   "    register_aggregate(name, #inputs, func, agid) "
   "  end "
   "  if interface == [[table]] then "
   "    local realf = func "
   "    if #inputs > 0 then "
   "      realf = function(...) "
   "        local nargs = select([[#]], ...) "
   "        local inner = coroutine.create(func) "
   "        while true do "
   "          local rvs = {coroutine.resume(inner, ...)} "
   "          if not rvs[1] then "
   "            error([[resume failed]]) "
   "          end "
   "          if coroutine.status(inner) == [[dead]] then "
   "            return "
   "          end "
   "          for i=1,nargs do "
   "            rvs[#rvs+1] = select(i, ...) "
   "          end "
   "          coroutine.yield(table.unpack(rvs, 2)) "
   "        end "
   "      end "
   "    end "     
   "    register_table(name, inputs, outputs, realf) "
   "  end "
   " "  
   "  local u = { "
   "    string.format([=[[\"%s\"]]=], table.concat(inputs, [[\",\"]])), "
   "    string.format([=[[\"%s\"]]=], table.concat(outputs, [[\",\"]])), "
   "    interface, "
   "    src "
   "  } "
   " "
   "  t[name] = u "
   "  updated = true "
   "end " 
   " "
   "local function iter() "
   "  local done = {} "
   "  updated = true "
   "  while updated do "
   "    updated = false "
   "    for k, v in pairs(t) do "
   "      if not done[k] then "
   "        done[k] = true "
   "        coroutine.yield(k, table.unpack(v, 1, 4)) "
   "      end "
   "    end "
   "  end "
   "end "
   " "
   "return insert, iter";

  int status = luaL_loadstring(l, luacode);
  assert(status == LUA_OK);

  lua_call(l, 0, 2);

  int iteridx = luaL_ref(l, LUA_REGISTRYINDEX);
  int insertidx = luaL_ref(l, LUA_REGISTRYINDEX);

  lua_pushinteger(l, insertidx);
  lua_pushinteger(l, iteridx);

  return 2;
}

static int ft_connect(sqlite3 *db, void *unused, int argc, const char * const * unused2,
 sqlite3_vtab **pvtab, char **perr)
{
  if(argc != 3)
  {
    return SQLITE_ERROR;
  }

  const char *tbldef =
   "create table t("
   "  name text primary key,"
   "  args text,"
   "  returns text,"
   "  type text,"
   "  src text"
   ") without rowid";

  sqlite3_declare_vtab(db, tbldef);

  rc_lua_state *rcs = NULL;
  rc_lua_get(&rcs);

  lua_pushcfunction(rcs->l, setupstate);
  lua_pushlightuserdata(rcs->l, rcs);
  lua_pushlightuserdata(rcs->l, db);
  if(lua_pcall(rcs->l, 2, 2, 0) != LUA_OK)
  {
    assert(lua_type(rcs->l, -1) == LUA_TSTRING);
    *perr = sqlite3_mprintf("failure to create vt: %s",
     lua_tostring(rcs->l, -1));
    goto cleanup;
  }

  ft_vtab *vt = (ft_vtab *)sqlite3_malloc(sizeof(ft_vtab));
  vt->rcs = rcs;
  vt->insert = (int)lua_tointeger(rcs->l, -2);
  vt->iter = (int)lua_tointeger(rcs->l, -1);
  *pvtab = (sqlite3_vtab *)vt;

  lua_pop(rcs->l, 2);

  return SQLITE_OK;
cleanup:
  rc_lua_release(&rcs);
  return SQLITE_ERROR;
}

static int ft_disconnect(sqlite3_vtab *vt)
{
  ft_vtab *ft_vt = (ft_vtab *)vt;
  rc_lua_release(&ft_vt->rcs);
  sqlite3_free(ft_vt);
  return SQLITE_OK;
}

static int ft_bestIndex(sqlite3_vtab *unused, sqlite3_index_info *unused2)
{
  return SQLITE_OK;
}

static int ft_rename(sqlite3_vtab *unused, const char *unused2)
{
  return SQLITE_OK;
}

typedef struct
{
  sqlite3_vtab_cursor cur;
  coro_state cs;
} ft_cursor;

static int ft_open(sqlite3_vtab *unused, sqlite3_vtab_cursor **pcur)
{
  ft_cursor *cur = (ft_cursor *)sqlite3_malloc(sizeof(ft_cursor));
  if(cur == NULL)
  {
    return SQLITE_NOMEM;
  }

  rc_lua_initcoro(&cur->cs);
  *pcur = (sqlite3_vtab_cursor *)cur;
  return SQLITE_OK;
}

static int ft_close(sqlite3_vtab_cursor *cursor)
{
  ft_cursor *cur = (ft_cursor *)cursor;
  ft_vtab *vt = (ft_vtab *)(cur->cur.pVtab);
  rc_lua_releasecoro(vt->rcs, &cur->cs);

  sqlite3_free(cur);
  return SQLITE_OK;
}

static int ft_filter(sqlite3_vtab_cursor *cursor, int unused, const char * unused2,
 int unused3, sqlite3_value **unused4)
{
//  fprintf(stderr, "Begin Filter\n");
  ft_cursor *cur = (ft_cursor *)cursor;
  ft_vtab *vt = (ft_vtab *)(cur->cur.pVtab);

  rc_lua_obtaincoro(vt->rcs, &cur->cs);
  
  lua_rawgeti(cur->cs.coro, LUA_REGISTRYINDEX, vt->iter);
  assert(lua_type(cur->cs.coro, 1) == LUA_TFUNCTION);
  int rc = lua_resume(cur->cs.coro, NULL, 0);
  if(rc != LUA_OK && rc != LUA_YIELD)
  {
    return SQLITE_ERROR;
  }

  return SQLITE_OK;
}
  
static int ft_next(sqlite3_vtab_cursor *cursor)
{
//  fprintf(stderr, "Begin Next\n");
  ft_cursor *cur = (ft_cursor *)cursor;
  if(lua_status(cur->cs.coro) != LUA_YIELD)
  {
    return SQLITE_ERROR;
  }

  lua_settop(cur->cs.coro, 0);
  int rc = lua_resume(cur->cs.coro, NULL, 0);
  if(rc != LUA_OK && rc != LUA_YIELD)
  {
    return SQLITE_ERROR;
  }

  return SQLITE_OK;
}

static int ft_eof(sqlite3_vtab_cursor *cursor)
{
//  fprintf(stderr, "Begin Eof\n");
  ft_cursor *cur = (ft_cursor *)cursor;
  return lua_status(cur->cs.coro) == LUA_OK;
}

static int ft_rowid(sqlite3_vtab_cursor *unused, sqlite_int64 *unused2)
{
  return SQLITE_ERROR;
}

static int ft_column(sqlite3_vtab_cursor *cursor, sqlite3_context *ctx, int cidx)
{
//  fprintf(stderr, "Requesting column %d\n", cidx);

  ft_cursor *cur = (ft_cursor *)cursor;
  ++cidx;
  assert(cidx >= 1 && cidx <= lua_gettop(cur->cs.coro));

  switch(lua_type(cur->cs.coro, cidx))
  {
    case LUA_TNIL:
      sqlite3_result_null(ctx);
      break;
    case LUA_TBOOLEAN:
      sqlite3_result_int(ctx, lua_toboolean(cur->cs.coro, cidx));
      break;
    case LUA_TSTRING:
      sqlite3_result_text(ctx, lua_tostring(cur->cs.coro, cidx), -1,
       SQLITE_TRANSIENT);
      break;
    case LUA_TNUMBER:
      if(lua_isinteger(cur->cs.coro, cidx))
      {
        sqlite3_result_int64(ctx, lua_tointeger(cur->cs.coro, cidx));
      }
      else
      {
        sqlite3_result_double(ctx, lua_tonumber(cur->cs.coro, cidx));
      }
      break;
    default:
      sqlite3_result_text(ctx, "can't convert type", -1, SQLITE_STATIC);
      return SQLITE_ERROR;
  }

  return SQLITE_OK;
}

static int ft_update(sqlite3_vtab *vtab, int argc, sqlite3_value **argv,
 sqlite_int64 *rowid)
{
  ft_vtab *vt = (ft_vtab *)vtab;

  //We only implement insert, all other operations result in an SQLITE_ERROR
  if(argc != 2 + 5)
  {
    return SQLITE_ERROR;
  }
  if(sqlite3_value_type(argv[0]) != SQLITE_NULL)
  {
    return SQLITE_ERROR;
  }

  //The name(2) and src(6) should be text, everything else should be null
  if(sqlite3_value_type(argv[2]) != SQLITE_TEXT)
  {
    return SQLITE_CONSTRAINT;
  }
  for(int i=3;i!=6;++i)
  {
    if(sqlite3_value_type(argv[i]) != SQLITE_NULL)
    {
      return SQLITE_CONSTRAINT;
    }
  }
  if(sqlite3_value_type(argv[6]) != SQLITE_TEXT)
  {
    return SQLITE_CONSTRAINT;
  }

  coro_state setup;
  rc_lua_initcoro(&setup);
  rc_lua_obtaincoro(vt->rcs, &setup);

  lua_rawgeti(setup.coro, LUA_REGISTRYINDEX, vt->insert);
  if(rc_lua_pushstring(setup.coro, sqlite3_value_text(argv[2])) != LUA_OK
   || rc_lua_pushstring(setup.coro, sqlite3_value_text(argv[6])) != LUA_OK)
  {
    return SQLITE_ERROR;
  }

  //Just in case some pratt does a coroutine.yield inside the setup, we should
  //resume, so we can catch it and fail
  int rc = SQLITE_OK;
  if(lua_resume(setup.coro, NULL, 2) != LUA_OK)
  {
    rc = SQLITE_CONSTRAINT;
  }

  rc_lua_releasecoro(vt->rcs, &setup);

  return rc;
}
  
static sqlite3_module ft_mod =
{
  3,
  NULL,
  ft_connect,
  ft_bestIndex,
  ft_disconnect,
  NULL,
  ft_open,
  ft_close,
  ft_filter,
  ft_next,
  ft_eof,
  ft_column,
  ft_rowid,
  ft_update,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  ft_rename,
  NULL,
  NULL,
  NULL,
  NULL
};
     
int setup_functiontable(sqlite3 *db)
{
  return sqlite3_create_module(db, "luafunctions", &ft_mod, NULL);
}
