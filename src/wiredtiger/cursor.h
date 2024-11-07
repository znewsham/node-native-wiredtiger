#include <wiredtiger.h>
#include "types.h"
#include "helpers.h"
#include <avcall.h>

#include <vector>
using namespace std;

#ifndef WIREDTIGER_CURSOR_H
#define WIREDTIGER_CURSOR_H

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
  class Cursor {
    protected:
      bool isInitted = false;
      bool isRaw = false;
      std::vector<Format> keyValueFormats;
      std::vector<Format> valueValueFormats;
      int init();

    public:
      WT_CURSOR* cursor;

      Cursor(WT_CURSOR* _cursor): cursor(_cursor) {

      }

      virtual ~Cursor() {
        if (value != NULL) {
          freeQueryValues(value, columnCount(true));
          value = NULL;
        }
        if (key != NULL) {
          freeQueryValues(key, columnCount(false));
          key = NULL;
        }
      }

      int search();
      int searchNear(int* exact);
      int update();
      int insert();
      int remove();
      int next();
      int prev();
      int close();
      int reset();
      int bound(char* config);
      int compare(Cursor* other, int* comp);
      int equals(Cursor* other, int* equals);
      void setKey(QueryValueOrWT_ITEM* valueArray);
      void setValue(QueryValueOrWT_ITEM* valueArray);
      int getValue(QueryValueOrWT_ITEM* valueArray);
      int getKey(QueryValueOrWT_ITEM* valueArray);
      QueryValueOrWT_ITEM* initArray(bool forValues, size_t* size);
      virtual Format formatAt(bool forValues, int i);
      size_t columnCount(bool forValues);

      virtual const char* getUri() {
        return this->cursor->uri;
      }

      virtual const char* getKeyFormat() {
        return this->cursor->key_format;
      }

      virtual const char* getValueFormat() {
        return this->cursor->value_format;
      }

      virtual WT_SESSION* getRawSession() {
        return this->cursor->session;
      }

    private:
      QueryValueOrWT_ITEM* value = NULL;
      QueryValueOrWT_ITEM* key = NULL;

      void populateInner(QueryValueOrWT_ITEM* valueArray, av_alist* argList, bool forValues, bool forWrite);

      int populate(
        int (*funcptr)(WT_CURSOR* cursor, ...),
        QueryValueOrWT_ITEM* valueArray,
        bool forValues
      );
      void populate(
        void (*funcptr)(WT_CURSOR* cursor, ...),
        QueryValueOrWT_ITEM* valueArray,
        bool forValues
      );

      int populateRaw(
        int (*funcptr)(WT_CURSOR* cursor, ...),
        QueryValueOrWT_ITEM* valueArray
      );

      void populateRaw(
        void (*funcptr)(WT_CURSOR* cursor, ...),
        QueryValueOrWT_ITEM* valueArray
      );
  };
}
#endif
