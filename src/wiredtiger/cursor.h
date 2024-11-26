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
      bool isClosed = false;
      std::vector<Format> keyValueFormats;
      std::vector<Format> valueValueFormats;
      WT_CURSOR* cursor;
      int init();

    public:

      Cursor(WT_CURSOR* _cursor): cursor(_cursor) {

      }

      void setExternal() {
        isExternal = true;
      }

      virtual ~Cursor() {
        if (!isClosed && isExternal) {
          this->close();
        }
      }

      inline WT_CURSOR* getWTCursor() {
        return this->cursor;
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
      void setKey(std::vector<QueryValue>* valueArray);
      void setValue(std::vector<QueryValue>* valueArray);
      int getValue(std::vector<QueryValue>* valueArray);
      int getKey(std::vector<QueryValue>* valueArray);
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

      uint64_t getFlags() {
        return this->cursor->flags;
      }

      void setFlags(uint64_t flags) {
        this->cursor->flags = flags;
      }

      virtual WT_SESSION* getRawSession() {
        return this->cursor->session;
      }

    private:
      bool isExternal = false; // proxy for whether we own the cursor or not (e.g., should be false for custom extractor cursor instances)
      void populateInner(std::vector<QueryValue>* valueArray, std::vector<WT_ITEM>* wtItems, av_alist* argList, bool forValues, bool forWrite);

      int populate(
        int (*funcptr)(WT_CURSOR* cursor, ...),
        std::vector<QueryValue>* valueArray,
        bool forValues
      );
      void populate(
        void (*funcptr)(WT_CURSOR* cursor, ...),
        std::vector<QueryValue>* valueArray,
        bool forValues
      );

      int populateRaw(
        int (*funcptr)(WT_CURSOR* cursor, ...),
        std::vector<QueryValue>* valueArray
      );

      void populateRaw(
        void (*funcptr)(WT_CURSOR* cursor, ...),
        std::vector<QueryValue>* valueArray
      );
  };
}
#endif
