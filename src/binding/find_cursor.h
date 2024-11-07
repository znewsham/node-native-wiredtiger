#include <node.h>
#include "v8-version.h"
#include "../wiredtiger/find_cursor.h"
#include "../helpers.h"
#include "./helpers.h"
#ifndef NODE_FIND_CURSOR_H
#define NODE_FIND_CURSOR_H
namespace wiredtiger::binding {
  void FindCursorInit(Local<Object> target);
  Local<Object> FindCursorGetNew(char* tableName, std::vector<QueryCondition>* conditions, FindOptions* options);
  void FindCursorExtractFindOptions(Local<Object> optionsIn, FindOptions* optionsOut);
}
#endif
