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

  MultiCursor::MultiCursor(WiredTigerSession* session, char* tableName, std::vector<QueryCondition>* conditions): Cursor(NULL) {
    int joinCursorLength = strlen(tableName) + 12;
    int tableCursorLength = strlen(tableName) + 7;
    char* tableCursorUri = (char*)malloc(tableCursorLength);
    char* joinCursorUri = (char*)malloc(joinCursorLength);
    sprintf(joinCursorUri, "join:table:%s", tableName);
    sprintf(tableCursorUri, "table:%s", tableName);

    // TODO: throw on error somehow?
    cursorForConditions(
      session->getWTSession(),
      tableCursorUri,
      joinCursorUri,
      conditions,
      &cursor,
      false,
      false,
      &cursors,
      &buffers
    );
    free(joinCursorUri);
    free(tableCursorUri);
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
    this->seenVectorKeys.clear();
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

  int MultiCursor::next(std::vector<QueryValueOrWT_ITEM>** keyArray) {
    int error;
    if (isComplete) {
      return WT_NOTFOUND;
    }
    while ((error = Cursor::next()) == 0) {
      *keyArray = new std::vector<QueryValueOrWT_ITEM>(columnCount(false));
      RETURN_IF_ERROR(this->getKey(*keyArray));
      int count = seenVectorKeys.count({ *keyArray });
      if (count != 0) {
        delete *keyArray;
        continue;
      }
      seenVectorKeys.insert({ *keyArray });
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

  int MultiCursor::count(int* counter) {
    int error = 0;
    if (isComplete) {
      return WT_NOTFOUND;
    }
    std::vector<QueryValueOrWT_ITEM>* keyArray = NULL;
    while((error = this->next(&keyArray)) == 0) {
      *counter += 1;
    }
    for (auto iterator = seenVectorKeys.begin(); iterator != seenVectorKeys.end(); iterator++) {
      delete iterator->items;
    }
    return error;
  }


  int MultiCursor::next(std::vector<QueryValueOrWT_ITEM>** keyArray, std::vector<QueryValueOrWT_ITEM>** valueArray) {
    int error;
    if (isComplete) {
      return WT_NOTFOUND;
    }
    while ((error = Cursor::next()) == 0) {
      *keyArray = new std::vector<QueryValueOrWT_ITEM>(columnCount(false));
      RETURN_IF_ERROR(this->getKey(*keyArray));
      int count = seenVectorKeys.count({ *keyArray });
      if (count != 0) {
        delete *keyArray;
        continue;
      }
      seenVectorKeys.insert({ *keyArray });
      *valueArray = new std::vector<QueryValueOrWT_ITEM>(columnCount(true));
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
