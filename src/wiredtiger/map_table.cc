#include <wiredtiger.h>
#include "helpers.h"
#include "../helpers.h"
#include "map_table.h"

#include <vector>

using namespace std;

namespace wiredtiger {
  int WiredTigerMapTable::get(char* key, char** value) {
    ErrorAndResult<WT_CURSOR*> cursorResult = getCursor(cacheCursor);
    if (cursorResult.error != 0) {
      return cursorResult.error;
    }
    WT_CURSOR* cursor = cursorResult.value;
    cursor->set_key(cursor, key);
    RETURN_IF_ERROR(cursor->search(cursor));
    RETURN_IF_ERROR(cursor->get_value(cursor, value));
    RETURN_IF_ERROR(closeOrResetCursor(cursor));
    return 0;
  }

  int WiredTigerMapTable::set(char* key, char* value) {
    ErrorAndResult<WT_CURSOR*> cursorResult = getCursor(cacheCursor);
    if (cursorResult.error != 0) {
      return cursorResult.error;
    }
    WT_CURSOR* cursor = cursorResult.value;
    cursor->set_key(cursor, key);
    cursor->set_value(cursor, value);
    RETURN_IF_ERROR(cursor->update(cursor));
    RETURN_IF_ERROR(closeOrResetCursor(cursor));
    return 0;
  }

  int WiredTigerMapTable::del(char* key) {
    ErrorAndResult<WT_CURSOR*> cursorResult = getCursor(cacheCursor);
    if (cursorResult.error != 0) {
      return cursorResult.error;
    }
    WT_CURSOR* cursor = cursorResult.value;
    cursor->set_key(cursor, key);
    RETURN_IF_ERROR(cursor->remove(cursor));
    RETURN_IF_ERROR(closeOrResetCursor(cursor));
    return 0;
  }

  int WiredTigerMapTable::list(
    char* keyPrefix,
    int limit,
    std::vector<std::string>* results
  ) {
    WT_CURSOR* cursor;
    // a NULL keyPrefix means "get me all keys" - so that's what we're gonna do.
    // as such a standard cursor will work (we don't need to use a join or bounds)
    ErrorAndResult<WT_CURSOR*> cursorResult = getCursor(cacheCursor);
    if (cursorResult.error != 0) {
      return cursorResult.error;
    }
    cursor = cursorResult.value;
    if (keyPrefix != NULL) {
      int keyPrefixLength = strlen(keyPrefix);
      char* keyUpperBound = (char*)malloc(keyPrefixLength + 1);
      strcpy(keyUpperBound, keyPrefix);
      keyUpperBound[keyPrefixLength - 1]++;
      cursor->set_key(cursor, keyPrefix);
      RETURN_IF_ERROR(cursor->bound(cursor, "action=set,bound=lower,inclusive=1"));
      cursor->set_key(cursor, keyUpperBound);
      RETURN_IF_ERROR(cursor->bound(cursor, "action=set,bound=upper,inclusive=0"));
      free(keyUpperBound);
    }
    int err = 0;
    int records = 0;
    while ((err = cursor->next(cursor)) == 0) {
      if (records++ == limit) {
        break;
      }
      char* key;
      RETURN_IF_ERROR(cursor->get_key(cursor, &key));
      results->push_back(std::string(key));
    };
    return 0;
  }
}
