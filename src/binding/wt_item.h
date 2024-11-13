#include <node.h>
#include "v8-version.h"
#include "../wiredtiger/find_cursor.h"
#include "../helpers.h"
#include "./helpers.h"
#ifndef NODE_WT_ITEM_H
#define NODE_WT_ITEM_H
namespace wiredtiger::binding {
  void WtItemInit(Local<Object> target);
}
#endif
