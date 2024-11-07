#include <node.h>
#include "v8-version.h"
#include "../wiredtiger/find_cursor.h"
#include "../helpers.h"
#include "./helpers.h"
#ifndef NODE_TABLE_H
#define NODE_TABLE_H
namespace wiredtiger::binding {
  void WiredTigerTableInit(Local<Object> target);
}
#endif
