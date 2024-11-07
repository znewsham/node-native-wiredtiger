#include <wiredtiger.h>
#include "helpers.h"
#include "types.h"
#include <set>
#include "../helpers.h"

#include <vector>
#include <cstring>
#include "find_cursor.h"
using namespace std;

namespace wiredtiger {
  int FindCursor::init() {
    RETURN_IF_ERROR(Cursor::init());
    if (didRequestKeyFormat && requestedKeyFormat.size() != this->columnCount(false)) {
      return INVALID_VALUE_TYPE;
    }
    if (didRequestValueFormat && requestedValueFormat.size() != this->columnCount(true)) {
      return INVALID_VALUE_TYPE;
    }
    return 0;
  }

  int FindCursor::nextBatch(int batchSize, std::vector<std::unique_ptr<EntryOfVectors>>* results) {
    int error;
    if (isComplete) {
      return WT_NOTFOUND;
    }
    if (isReset) {
      RETURN_IF_ERROR(this->initCursor());

      if (!isInitted) {
        RETURN_IF_ERROR(this->init());
      }
      isReset = false;
    }
    int fetched = 0;
    metrics.batchesRequested++;
    std::vector<QueryValueOrWT_ITEM>* keyArray = NULL;
    std::vector<QueryValueOrWT_ITEM>* valueArray = NULL;
    while (fetched++ < batchSize && (error = MultiCursor::next(&keyArray, &valueArray)) == 0) {
      metrics.valuesSeen++;
      metrics.returned++;
      EntryOfVectors* ev = new EntryOfVectors();
      results->push_back(std::unique_ptr<EntryOfVectors>(ev));
      ev->keyArray = keyArray;
      ev->valueArray = valueArray;
    }
    if (error == WT_NOTFOUND) {
      isComplete = true;
    }
    return 0;
  }

  Format FindCursor::formatAt(bool forValues, int i) {
    if (isRaw || (forValues && !didRequestValueFormat) || (!forValues && !didRequestKeyFormat)) {
      return Cursor::formatAt(forValues, i);
    }
    if (forValues) {
      return this->requestedValueFormat.at(i);
    }
    return this->requestedKeyFormat.at(i);
  }

  int FindCursor::initCursor() {
    int joinCursorLength = strlen(this->tableName) + 12;
    int tableCursorLength = strlen(this->tableName) + 7;
    if (columns != NULL) {
      int columnLength = strlen(columns);
      joinCursorLength += columnLength;
      tableCursorLength += columnLength;
    }
    char* tableCursorUri = (char*)malloc(tableCursorLength);
    char* joinCursorUri = (char*)malloc(joinCursorLength);
    if (columns != NULL) {
      sprintf(joinCursorUri, "join:table:%s%s", this->tableName, columns);
      sprintf(tableCursorUri, "table:%s%s", this->tableName, columns);
    }
    else {
      sprintf(joinCursorUri, "join:table:%s", this->tableName);
      sprintf(tableCursorUri, "table:%s", this->tableName);
    }
    int result = cursorForConditions(
      this->session->session,
      tableCursorUri,
      joinCursorUri,
      conditions,
      &cursor,
      false,
      false,
      &cursors,
      &buffers
    );

    free(tableCursorUri);
    free(joinCursorUri);
    return result;
  }


  FindCursor::FindCursor(char* _tableName, std::vector<QueryCondition>* _conditions, FindOptions* options):
    MultiCursor(NULL),// TODO? We're passing a null cursor to the parent class. This is currently fine since we overload the methods,
    conditions(_conditions),
    tableName(_tableName)
  {
    session = options->session;
    if (options->keyFormat != NULL) {
      char* keyFormat = (char*)calloc(sizeof(char), strlen(options->keyFormat) + 1);
      strcpy(keyFormat, options->keyFormat);
      parseFormat(keyFormat, &requestedKeyFormat);
      didRequestKeyFormat = true;
      free(keyFormat);
    }
    if (options->valueFormat != NULL) {
      char* valueFormat = (char*)calloc(sizeof(char), strlen(options->valueFormat) + 1);
      strcpy(valueFormat, options->valueFormat);
      parseFormat(valueFormat, &requestedValueFormat);
      didRequestValueFormat = true;
      free(valueFormat);
    }
    if (options->columns != NULL) {
      columns = (char*)calloc(sizeof(char), strlen(options->columns) + 1);
      strcpy(columns, options->columns);
    }
  }

  FindCursor::~FindCursor() {
    delete conditions;
    free(tableName);
    if (columns != NULL) {
      free(columns);
    }
    // Cursor::~Cursor();
  }

}
