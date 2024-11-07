#include <node.h>
#include "v8-version.h"
#include "../wiredtiger/find_cursor.h"
#include "../helpers.h"
#include "./helpers.h"
#ifndef NODE_DB_H
#define NODE_DB_H
namespace wiredtiger::binding {
  void WiredTigerDBInit(Local<Object> target);
}
#endif
