#include <node.h>
#include "v8-local-handle.h"
#include "v8-value.h"
#include <wiredtiger.h>

using namespace v8;
#ifndef NODE_CFG_ITEM_H
#define NODE_CFG_ITEM_H
namespace wiredtiger::binding {
  Local<Value> GetConfigItemValue(WT_CONFIG_ITEM* appcfg);
}
#endif
