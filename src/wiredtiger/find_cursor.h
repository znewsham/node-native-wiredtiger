#include <wiredtiger.h>
#include "types.h"
#include "helpers.h"
#include "session.h"
#include "cursor.h"
#include "multi_cursor.h"
#include <string_view>
#include <set>
#include <memory>

#include <vector>
using namespace std;

#ifndef WIREDTIGER_FIND_CURSOR_H
#define WIREDTIGER_FIND_CURSOR_H

// TODO sync these with the JS side via an export or similar.
#define OPERATION_EQ 0
#define OPERATION_NE 1
#define OPERATION_GE 2
#define OPERATION_LE 3
#define OPERATION_GT 4
#define OPERATION_LT 5

#define OPERATION_AND 6
#define OPERATION_OR 7

#define OPERATION_INDEX 8

namespace wiredtiger {
  typedef struct FindOptions {
    char* keyFormat;
    char* valueFormat;
    char* columns;
    WiredTigerSession* session;
  } FindOptions;
  class FindCursor : public MultiCursor{
    private:
      char* columns = NULL;
      std::vector<QueryCondition>* conditions;
      char* tableName = NULL;

      // TODO: can these be collapsed into one? Do we really care about the official cursor format if we've specified it? E.g., do we ever give the format into a WT method?
      std::vector<Format> requestedKeyFormat;
      bool didRequestKeyFormat = false;
      std::vector<Format> requestedValueFormat;
      bool didRequestValueFormat = false;

    protected:
      int init();
    public:
      int initCursor(); // public just for accessors? Puke.
      WiredTigerSession *session;
      Format formatAt(bool forValues, int i);
      int nextBatch(int batchSize, std::vector<std::unique_ptr<EntryOfVectors>>* results);

      const char* getUri() {
        initCursor();
        return this->cursor->uri;
      }

      const char* getKeyFormat() {
        initCursor();
        return this->cursor->key_format;
      }

      const char* getValueFormat() {
        initCursor();
        return this->cursor->value_format;
      }

      virtual WT_SESSION* getRawSession() {
        return this->session->session;
      }


      FindCursor(char* tableName, std::vector<QueryCondition>* _conditions, FindOptions* options);
      ~FindCursor();
  };
}
#endif
