#include <sqlite3ext.h>
SQLITE_EXTENSION_INIT1

int setup_executelua(sqlite3 *db);
int setup_functiontable(sqlite3 *db);

int sqlite3_luafunctions_init(sqlite3 *db, char **pzErrMsg,
 sqlite3_api_routines *pApi)
{
  SQLITE_EXTENSION_INIT2(pApi);

  int rc = SQLITE_OK;
  setup_functiontable(db);
  return rc;
}
