#include <node.h>
#include <wiredtiger.h>
#include "helpers.h"
#include "wiredtiger/find_cursor.h"
#include "wiredtiger/session.h"
#include "wiredtiger/db.h"
#include "binding/find_cursor.h"
#include "binding/session.h"
#include "binding/cursor.h"
#include "binding/table.h"
#include "binding/db.h"
#include "binding/map.h"
#include "binding/map_table.h"
#include "binding/custom_extractor.h"
#include "binding/custom_collator.h"
#include "binding/wt_item.h"
#include "v8-version.h"

#include <assert.h>
#include <node_version.h>
#include <node_buffer.h>

#include <vector>
#include <iostream>

using namespace std;
using namespace v8;
using namespace node;

namespace wiredtiger {

#define MAX_COLUMNS 10

void init(Local<Object> exports, v8::Local<v8::Value> module, void* priv) {
  (void)module;
  (void)priv;
  wiredtiger::binding::WiredTigerDBInit(exports);
  wiredtiger::binding::WiredTigerSessionInit(exports);
  wiredtiger::binding::FindCursorInit(exports);
  wiredtiger::binding::CursorInit(exports);
  wiredtiger::binding::MapInit(exports);
  wiredtiger::binding::WiredTigerTableInit(exports);
  wiredtiger::binding::WiredTigerMapTableInit(exports);
  wiredtiger::binding::CustomExtractorInit(exports);
  wiredtiger::binding::CustomCollatorInit(exports);
  wiredtiger::binding::WtItemInit(exports);
}

NODE_MODULE(NODE_GYP_MODULE_NAME, init)

}  // namespace demo
