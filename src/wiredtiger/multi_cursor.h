#include <wiredtiger.h>
#include "types.h"
#include "helpers.h"
#include "session.h"
#include "cursor.h"
#include <string_view>
#include <set>

#include <vector>
using namespace std;

#ifndef WIREDTIGER_MULTI_CURSOR_H
#define WIREDTIGER_MULTI_CURSOR_H

namespace wiredtiger {
  typedef struct CursorMetrics {
    uint32_t batchesRequested = 0;

    // for now, these will be the same - since we dont:
    // 1. support queries over non index fields
    // 2. support non index range queries
    // 3. deduplicate index keys.
    uint32_t keysSeen = 0;
    uint32_t valuesSeen = 0;
    uint32_t returned = 0;
  } CursorMetrics;

  typedef struct UniqueKey {
    QueryValueOrWT_ITEM* items;
    size_t size;
  } UniqueKey;

  struct UniqueKeyCmp {
    bool operator()(UniqueKey a, UniqueKey b) const {
      // since we only care about uniqueness not any strict ordering, we don't have to be super careful here.
      if (b.size != a.size) {
        return a.size > b.size;
      }
      for (size_t i = 0; i < a.size; i++) {
        // TODO: use the collator? Is that even possible having unpacked the keys?
        QueryValue aQV = a.items[i].queryValue;
        QueryValue bQV = b.items[i].queryValue;
        if (aQV.dataType != bQV.dataType) {
          return (int)aQV.dataType - (int)bQV.dataType;
        }
        int cmp;
        if (
          aQV.dataType == FIELD_CHAR_ARRAY
        ) {
          // incredibly, it looks like wiredtiger doesn't 0 out the bytes of a char array (even though they claim they do)
          cmp = strncmp(
            (char*)aQV.value,
            (char*)bQV.value,
            max(
              strnlen((char*)aQV.value, (size_t)aQV.size),
              strnlen((char*)bQV.value, (size_t)aQV.size)
            )
          );
          if (cmp != 0) {
            return cmp > 0;
          }
          continue;
        }
        if(aQV.dataType == FIELD_WT_ITEM
          || aQV.dataType == FIELD_WT_UITEM
          || aQV.dataType == FIELD_WT_ITEM_BIGINT
          || aQV.dataType == FIELD_WT_ITEM_DOUBLE
        ) {
          cmp = memcmp(aQV.value, bQV.value, aQV.size);
          if (cmp != 0) {
            return cmp > 0;
          }
          continue;
        }
        if (aQV.dataType == FIELD_STRING) {
          cmp = strcmp((char*)aQV.value, (char*)bQV.value);
          if (cmp != 0) {
            return cmp > 0;
          }
          continue;
        }

        // everything else is a number
        if (aQV.value != bQV.value) {
          return ((uint64_t)aQV.value - (uint64_t)bQV.value) > 0;
        }
      }

      return 0;
    }
  };


  class MultiCursor : public Cursor{
    private:
      set<UniqueKey, UniqueKeyCmp> seenKeys;
    protected:
      bool isReset = true;
      bool isComplete = false;
      std::vector<WT_CURSOR*> cursors;
      std::vector<void*> buffers;
      CursorMetrics metrics;
    public:
      MultiCursor(WT_CURSOR* cursor);
      virtual ~MultiCursor();

      int next(QueryValueOrWT_ITEM** keys, size_t* keySize, QueryValueOrWT_ITEM** values, size_t* valueSize);
      int reset();
      int close();
  };
}

#endif