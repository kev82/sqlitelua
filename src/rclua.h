#ifndef __REF_COUNTED_LUA_HEADER__
#define __REF_COUNTED_LUA_HEADER__

typedef struct lua_State lua_State;

typedef struct
{
  lua_State *l;
  int rc;
} rc_lua_state;

void rc_lua_get(rc_lua_state **prcs);
void rc_lua_release(rc_lua_state **prcs);

int rc_lua_pushstring(lua_State *unsafe, const char *str);

typedef struct
{
  lua_State *coro;
  int regIdx;
} coro_state;

void rc_lua_initcoro(coro_state *c);
void rc_lua_obtaincoro(rc_lua_state *rcs, coro_state *c);
void rc_lua_releasecoro(rc_lua_state *rcs, coro_state *c);

#endif
