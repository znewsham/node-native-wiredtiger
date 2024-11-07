#include <wiredtiger.h>
#include "helpers.h"
#include "table.h"
#include "../helpers.h"

#include <cstring>
#include <vector>

using namespace std;

#ifndef WIREDTIGER_MAP_TABLE_H
#define WIREDTIGER_MAP_TABLE_H

namespace wiredtiger {
  class WiredTigerMapTable: public WiredTigerTable {
    private:
      bool cacheCursor = true;
      WT_CURSOR *cachedCursor = NULL;
      WT_SESSION *session = NULL;

      ErrorAndResult<WT_CURSOR*> getCursor(bool useCache) {
        int error = 0;
        if (session == NULL) {
          error = db->conn->open_session(db->conn, NULL, "cache_cursors", &session);
          initTable(new WiredTigerSession(session));
        }
        if (error) {
          return { error, cachedCursor };
        }
        if (useCache && cachedCursor != NULL) {
          return ErrorAndResult<WT_CURSOR*> { 0, cachedCursor };
        }
        WT_CURSOR* cursor;
        error = session->open_cursor(this->session, specName, NULL, NULL, &cursor);
        if (useCache) {
          cachedCursor = cursor;
        }
        if (error) {
          printf("Error creating cursor: %d %s\n", error, specName);
        }
        return ErrorAndResult<WT_CURSOR*> { error, cursor };
      }

      int closeOrResetCursor(WT_CURSOR* cursor) {
        if (cacheCursor && cursor == cachedCursor) {
          return cursor->reset(cursor);
        }
        return cursor->close(cursor);
      }
    public:
      int get(char* key, char** value);

      int set(char* key, char* value);

      int del(char* key);

      int list(
        char* keyPrefix,
        int limit,
        std::vector<std::string>* results
      );

      WiredTigerMapTable(WiredTigerDB* db, char* tableName):
      WiredTigerTable::WiredTigerTable(db, tableName, (char*)"key_format=S,value_format=S") // TODO
      {

      }

      ~WiredTigerMapTable() {
        if (cachedCursor != NULL) {
          cachedCursor->close(cachedCursor);
        }
        if (session != NULL) {
          session->close(session, NULL);
        }
      }
  };
}
#endif
