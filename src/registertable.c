#include <sqlite3ext.h>
SQLITE_EXTENSION_INIT3;

#include <memory.h>
#include <lua.h>
#include <lauxlib.h>
#include <assert.h>

#include "rclua.h"

typedef struct
{
  rc_lua_state *rcs;
  int ninputs;
  int noutputs;
  int vtabidx;
  int funcidx;
} luatabledata;

typedef struct
{
  sqlite3_vtab vtab;
  luatabledata *ltd;
} f_vtab;

static int xConnect(sqlite3 *db, void *data, int argc, const char *const *argv,
 sqlite3_vtab **pvtab, char **pzerr)
{
  //fprintf(stderr, "connect\n");

  if(argc != 3) {
    return SQLITE_ERROR;
    //fprintf(stderr, "fail connect 1\n");
  }

  luatabledata *ltd = (luatabledata *)data;

  coro_state cs;
  rc_lua_initcoro(&cs);
  rc_lua_obtaincoro(ltd->rcs, &cs);

  lua_rawgeti(cs.coro, LUA_REGISTRYINDEX, ltd->vtabidx);
  lua_pushlightuserdata(cs.coro, db);
  int status = lua_pcall(cs.coro, 1, 0, 0);

  rc_lua_releasecoro(ltd->rcs, &cs);

  if(status != LUA_OK)
  {
    //fprintf(stderr, "fail connect 2\n");
    return SQLITE_ERROR;
  }

  f_vtab *ft = (f_vtab *)sqlite3_malloc(sizeof(f_vtab));
  if(ft == NULL)
  {
    //fprintf(stderr, "fail connect 3\n");
    return SQLITE_ERROR;
  }

  //fprintf(stderr, "connect success\n");

  ft->ltd = ltd;
  *pvtab = (sqlite3_vtab *)ft;
  return SQLITE_OK;
}

static int xDisconnect(sqlite3_vtab *vtab)
{
//  fprintf(stderr, "disconnect\n");
  sqlite3_free(vtab);
}

int xBestIndex(sqlite3_vtab *vtab, sqlite3_index_info *ii)
{
  //fprintf(stderr, "start bestindex\n");
  //I need to get the list of hidden columns
  //For each hidden column, I need to see if I can
  //find an equality constraint on it that is usable
  //if I do, I set the output fpr that constraint to the
  //index of the constraint
  //if for any I can't find the constraint, I return
  //sqlite_constraint, else I return sqlite_ok

  f_vtab *ft = (f_vtab *)vtab;
  int hco = ft->ltd->noutputs;

  for(int i=0;i!=ft->ltd->ninputs;++i)
  {
    int found = 0;
    for(int j=0;j!=ii->nConstraint;++j)
    {
      if(ii->aConstraint[j].iColumn == hco + i &&
       ii->aConstraint[j].op == SQLITE_INDEX_CONSTRAINT_EQ &&
       ii->aConstraint[j].usable != 0)
      {
        found = 1;
        ii->aConstraintUsage[j].argvIndex = i+1;
        ii->aConstraintUsage[j].omit = 1;
      }
    }

    if(!found)
    {
      //fprintf(stderr, "fail bestindex\n");
      return SQLITE_CONSTRAINT;
    }
  }

  //fprintf(stderr, "success bestindex\n");
  return SQLITE_OK;
}

typedef struct
{
  sqlite3_vtab_cursor cur;
  coro_state cs;
} f_cur;

static int xOpen(sqlite3_vtab *vtab, sqlite3_vtab_cursor **pcur)
{
  //fprintf(stderr, "open\n");
  f_cur *cur = (f_cur *)sqlite3_malloc(sizeof(f_cur));
  if(cur == NULL)
  {
    return SQLITE_NOMEM;
  }

  rc_lua_initcoro(&cur->cs);
  *pcur = (sqlite3_vtab_cursor *)cur;
  return SQLITE_OK;
}

static int xClose(sqlite3_vtab_cursor *cursor)
{
  //fprintf(stderr, "close\n");
  f_cur *cur = (f_cur *)cursor;
  f_vtab *ft = (f_vtab *)(cur->cur.pVtab);
  rc_lua_releasecoro(ft->ltd->rcs, &cur->cs);
  
  sqlite3_free(cur);
  return SQLITE_OK;
}

static int xEof(sqlite3_vtab_cursor *cursor)
{
  //fprintf(stderr, "eof\n");
  f_cur *cur = (f_cur *)cursor;
  return lua_status(cur->cs.coro) == LUA_OK;
}

static int xFilter(sqlite3_vtab_cursor *cursor, int idxnum,
 const char *idxStr, int argc, sqlite3_value **argv)
{
  //fprintf(stderr, "filter\n");
  f_cur *cur = (f_cur *)cursor;
  f_vtab *ft = (f_vtab *)(cur->cur.pVtab);

  rc_lua_obtaincoro(ft->ltd->rcs, &cur->cs);

  lua_rawgeti(cur->cs.coro, LUA_REGISTRYINDEX, ft->ltd->funcidx);
  assert(lua_type(cur->cs.coro, 1) == LUA_TFUNCTION);

  for(int i=0;i!=argc;++i)
  {
    switch(sqlite3_value_type(argv[i]))
    {
      case SQLITE_INTEGER:
        lua_pushinteger(cur->cs.coro, sqlite3_value_int64(argv[i]));
        break;
      case SQLITE_FLOAT:
        lua_pushnumber(cur->cs.coro, sqlite3_value_double(argv[i]));
        break;
      case SQLITE_TEXT:
        if(rc_lua_pushstring(cur->cs.coro, sqlite3_value_text(argv[i])) != LUA_OK)
        {
          return SQLITE_ERROR;
        }
        break;
      case SQLITE_NULL:
        lua_pushnil(cur->cs.coro);
        break;
      default:
        return SQLITE_ERROR;
    }
  }

  int rc = lua_resume(cur->cs.coro, NULL, argc);
  if(rc != LUA_OK && rc != LUA_YIELD)
  {
    return SQLITE_ERROR;
  }

  return SQLITE_OK;
}

static int xNext(sqlite3_vtab_cursor *cursor)
{
  //fprintf(stderr, "next\n");
  f_cur *cur = (f_cur *)cursor;
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

static int xColumn(sqlite3_vtab_cursor *cursor, sqlite3_context *ctx, int N)
{
  //fprintf(stderr, "column\n");
  f_cur *cur = (f_cur *)cursor;
  f_vtab *ft = (f_vtab *)(cur->cur.pVtab);

  int expectedCols = ft->ltd->noutputs + ft->ltd->ninputs;
  assert(N < expectedCols);
  assert(lua_gettop(cur->cs.coro) == expectedCols);

  ++N;

  switch(lua_type(cur->cs.coro, N))
  {
    case LUA_TNIL:
      sqlite3_result_null(ctx);
      break;
    case LUA_TNUMBER:
      sqlite3_result_double(ctx, lua_tonumber(cur->cs.coro, N));
      break;
    case LUA_TSTRING:
      sqlite3_result_text(ctx, lua_tostring(cur->cs.coro, N), -1, SQLITE_TRANSIENT);
      break;
    case LUA_TBOOLEAN:
      sqlite3_result_int(ctx, lua_toboolean(cur->cs.coro, N));
      break;
    default:
      sqlite3_result_text(ctx, "Unsupported return type", -1, SQLITE_STATIC);
      return SQLITE_ERROR;
      break;
  }
  return SQLITE_OK;
}

static int xRowid(sqlite3_vtab_cursor *cursor, sqlite_int64 *rowid)
{
  //fprintf(stderr, "rowid\n");
  assert(0);
  return SQLITE_ERROR;
}

static sqlite3_module modulefuncs = {
  1,
  NULL,
  xConnect,
  xBestIndex,
  xDisconnect,
  xDisconnect,
  xOpen,
  xClose,
  xFilter,
  xNext,
  xEof,
  xColumn,
  xRowid,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL
};

static void free_luatabledata(void *d)
{
  luatabledata *ltd = (luatabledata *)d;

  if(ltd->rcs)
  {
    lua_pushnil(ltd->rcs->l);
    lua_rawseti(ltd->rcs->l, LUA_REGISTRYINDEX, ltd->vtabidx);
    lua_pushnil(ltd->rcs->l);
    lua_rawseti(ltd->rcs->l, LUA_REGISTRYINDEX, ltd->funcidx);
    rc_lua_release(&ltd->rcs);
  }
}

static int auxdeclarevtab(lua_State *l)
{
  lua_settop(l, 1);
  luaL_checktype(l, 1, LUA_TLIGHTUSERDATA);

  assert(lua_type(l, lua_upvalueindex(1)) == LUA_TSTRING);

  sqlite3 *db = (sqlite3 *)lua_touserdata(l, 1);
  sqlite3_declare_vtab(db, lua_tostring(l, lua_upvalueindex(1)));
  return 0;
}

int register_tablefunc(lua_State *l)
{
  //name, ninputs, noutputs, vtab, func
  lua_settop(l, 5);
  luaL_checktype(l, 1, LUA_TSTRING);
  luaL_checktype(l, 2, LUA_TNUMBER);
  luaL_checktype(l, 3, LUA_TNUMBER);
  luaL_checktype(l, 4, LUA_TSTRING);
  luaL_checktype(l, 5, LUA_TFUNCTION);

  luatabledata *ltd = (luatabledata *)sqlite3_malloc(sizeof(luatabledata));
  if(ltd == NULL)
  {
    return luaL_error(l, "failed alloc");
  }
  memset(ltd, 0, sizeof(luatabledata));

  lua_pushvalue(l, 5);
  ltd->funcidx = luaL_ref(l, LUA_REGISTRYINDEX);

  lua_pushvalue(l, 4);
  lua_pushcclosure(l, auxdeclarevtab, 1);
  ltd->vtabidx = luaL_ref(l, LUA_REGISTRYINDEX);

  ltd->rcs = (rc_lua_state *)lua_touserdata(l, lua_upvalueindex(1));
  rc_lua_get(&ltd->rcs);

  ltd->ninputs = lua_tointeger(l, 2);
  ltd->noutputs = lua_tointeger(l, 3);

  sqlite3 *db = (sqlite3 *)lua_touserdata(l, lua_upvalueindex(2));

  if(sqlite3_create_module_v2(db, lua_tostring(l, 1), &modulefuncs, ltd,
   free_luatabledata) != SQLITE_OK)
  {
    return luaL_error(l, "module registration failed");
  }

  return 0;
}
