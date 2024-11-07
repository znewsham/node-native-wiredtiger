#include <node.h>
#include "v8-version.h"
#include "../wiredtiger/map_table.h"
#include "../helpers.h"
#include "helpers.h"
#ifndef NODE_MAP_TABLE_H
#define NODE_MAP_TABLE_H
namespace wiredtiger::binding {
  void WiredTigerMapTableInit(Local<Object> target);
}
#endif
