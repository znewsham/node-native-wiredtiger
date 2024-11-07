#include <wiredtiger.h>
#include "helpers.h"
#include "types.h"
#include <set>
#include "../helpers.h"

#include "multi_cursor.h"
using namespace std;

namespace wiredtiger {
  MultiCursor::MultiCursor(WT_CURSOR* cursor): Cursor(cursor) {

  }

  MultiCursor::~MultiCursor() {
    for (size_t i = 0; i < this->buffers.size(); i++) {
      free(this->buffers.at(i));
    }
    this->buffers.clear();
  }

  int MultiCursor::reset() {
    for (size_t i = 0; i < this->buffers.size(); i++) {
      free(this->buffers.at(i));
    }
    this->buffers.clear();
    for (size_t i = 0; i < this->cursors.size(); i++) {
      WT_CURSOR* nextCursor = this->cursors.at(i);
      RETURN_IF_ERROR(nextCursor->reset(nextCursor));
    }
    this->isComplete = false;
    this->isReset = true;
    this->seenKeys.clear();
    return 0;
  }

  int MultiCursor::close() {
    // this doesn't work - you have to close each cursor in turn, closing just the join doesn't close any sub cursors
    // or possibly closing a join doesn't close a subjoin (or a sub joins cursors?)
    // fuck it - I dunno, without this, sometimes it crashes on close
    // return this->cursor->close(this->cursor);

    for (size_t i = 0; i < this->cursors.size(); i++) {
      WT_CURSOR* nextCursor = this->cursors.at(i);
      RETURN_IF_ERROR(nextCursor->close(nextCursor));
    }
    return 0;
  }



  int MultiCursor::next(QueryValueOrWT_ITEM** keyArray, size_t* keySize, QueryValueOrWT_ITEM** valueArray, size_t* valueSize) {
    int error;
    if (isComplete) {
      return WT_NOTFOUND;
    }
    while ((error = Cursor::next()) == 0) {
      *keyArray = this->initArray(false, keySize);
      RETURN_IF_ERROR(this->getKey(*keyArray));
      int count = seenKeys.count({ *keyArray, *keySize });
      QueryValueOrWT_ITEM* key = *keyArray;
      if (count != 0) {
        continue;
      }
      seenKeys.insert({ *keyArray, *keySize });
      *valueArray = this->initArray(true, valueSize);
      RETURN_IF_ERROR(this->getValue(*valueArray));
      break;
    }

    if (error != 0) {
      if (error == WT_NOTFOUND) {
        isComplete = true;
      }
      return error;
    }
    return 0;
  }
}
