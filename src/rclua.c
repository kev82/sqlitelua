#include <sqlite3ext.h>
SQLITE_EXTENSION_INIT3

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <assert.h>
#include <memory.h>

#include "rclua.h"

static int auxsetupstate(lua_State *l)
{
  luaL_openlibs(l);

  lua_newthread(l);
  lua_rawsetp(l, LUA_REGISTRYINDEX, (void *)rc_lua_pushstring);
  return 0;
}

void rc_lua_get(rc_lua_state **prcs)
{
  if(*prcs == NULL)
  {
    *prcs = (rc_lua_state *)sqlite3_malloc(sizeof(rc_lua_state));
    assert(*prcs != NULL);
    (*prcs)->l = NULL;
    (*prcs)->rc = 0;
  }

  rc_lua_state *rcs = *prcs;

  assert(rcs->rc >= 0);
  if(rcs->rc > 0)
  {
    ++(rcs->rc);
  }
  else
  {
    rcs->l = luaL_newstate();
    lua_pushcfunction(rcs->l, auxsetupstate);
    if(lua_pcall(rcs->l, 0, 0, 0) != LUA_OK)
    {
      assert(0);
    }

    rcs->rc = 1;
  }
  assert(rcs->l != NULL);
}

void rc_lua_release(rc_lua_state **prcs)
{
  rc_lua_state *rcs = *prcs;

  assert(rcs->rc > 0);
  if(rcs->rc == 1)
  {
    lua_close(rcs->l);
    rcs->l = NULL;
  }
  --(rcs->rc);
  *prcs = NULL;
}

void rc_lua_initcoro(coro_state *c)
{
  c->coro = NULL;
  c->regIdx = -1;
}

static int auxobtaincoro(lua_State *l)
{
  lua_settop(l, 1);
  luaL_checktype(l, 1, LUA_TLIGHTUSERDATA);
  coro_state *c = (coro_state *)lua_touserdata(l, 1);

  if(c->coro != NULL && lua_status(c->coro) != LUA_OK)
  {
    lua_pushnil(l);
    lua_rawseti(l, LUA_REGISTRYINDEX, c->regIdx);
    rc_lua_initcoro(c);
  }

  if(c->coro == NULL)
  {
    c->coro = lua_newthread(l);
    c->regIdx = luaL_ref(l, LUA_REGISTRYINDEX);
  }

  assert(lua_status(c->coro) == LUA_OK);
  lua_settop(c->coro, 0);

  return 0;
}

void rc_lua_obtaincoro(rc_lua_state *rcs, coro_state *c)
{
  lua_pushcfunction(rcs->l, auxobtaincoro);
  lua_pushlightuserdata(rcs->l, c);
  if(lua_pcall(rcs->l, 1, 0, 0) != LUA_OK)
  {
    c->coro = NULL;
    c->regIdx = 0;
    assert(0);
  }
}

static int auxreleasecoro(lua_State *l)
{
  lua_settop(l, 1);
  luaL_checktype(l, 1, LUA_TLIGHTUSERDATA);
  coro_state *c = (coro_state *)lua_touserdata(l, 1);

  if(c->coro != NULL)
  {
    lua_pushnil(l);
    lua_rawseti(l, LUA_REGISTRYINDEX, c->regIdx);
  }
  rc_lua_initcoro(c);

  return 0;
}

void rc_lua_releasecoro(rc_lua_state *rcs, coro_state *c)
{
  lua_pushcfunction(rcs->l, auxreleasecoro);
  lua_pushlightuserdata(rcs->l, c);
  if(lua_pcall(rcs->l, 1, 0, 0) != LUA_OK)
  {
    c->coro = NULL;
    c->regIdx = 0;
    assert(0);
  }
}

static int auxpushstring(lua_State *l)
{
  luaL_checktype(l, 1, LUA_TLIGHTUSERDATA);
  lua_pushstring(l, (const char *)lua_touserdata(l, 1));
  return 1;
}
  
int rc_lua_pushstring(lua_State *unsafe, const char *str)
{
  if(!lua_checkstack(unsafe, 1))
  {
    return LUA_ERRMEM;
  }

  lua_rawgetp(unsafe, LUA_REGISTRYINDEX, rc_lua_pushstring);
  if(lua_type(unsafe, -1) != LUA_TTHREAD)
  {
    lua_pop(unsafe, 1);
    return LUA_ERRRUN;
  }

  lua_State *l = lua_tothread(unsafe, -1);
  lua_pop(unsafe, 1);
  if(lua_status(l) != LUA_OK || lua_gettop(l) != 0)
  {
    return LUA_ERRRUN;
  }

  lua_pushcfunction(l, auxpushstring);
  lua_pushlightuserdata(l, (void *)str);
  if(lua_pcall(l, 1, 1, 0) != LUA_OK)
  {
    lua_settop(l, 0);
    return LUA_ERRRUN;
  }

  assert(lua_type(l, -1) == LUA_TSTRING);
  lua_xmove(l, unsafe, 1);

  return LUA_OK;
}

static int auxdeleter(lua_State *l)
{
  lua_settop(l, 1);
  luaL_checktype(l, 1, LUA_TUSERDATA);

  deleter *d = (deleter *)lua_touserdata(l, 1);
  if(d->active)
  {
    (d->func)(d->data);
  }

  return 0;
}

deleter *rc_lua_deleter(lua_State *l)
{
  deleter *d = (deleter *)lua_newuserdata(l, sizeof(deleter));
  memset(d, 0, sizeof(deleter));

  lua_rawgetp(l, LUA_REGISTRYINDEX, (void *)rc_lua_deleter);
  if(lua_type(l, -1) == LUA_TNIL)
  {
    lua_createtable(l, 0, 1);
    lua_replace(l, -2);

    lua_pushstring(l, "__gc");
    lua_pushcfunction(l, auxdeleter);
    lua_settable(l, -3);
  }
  assert(lua_type(l, -1) == LUA_TTABLE);

  lua_setmetatable(l, -2);
  return d;
}
