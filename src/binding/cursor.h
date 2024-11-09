#include <node.h>
#include "v8-version.h"
#include "../helpers.h"
#include "helpers.h"
#ifndef NODE_CURSOR_H
#define NODE_CURSOR_H
namespace wiredtiger::binding {
  void CursorInit(Local<Object> target);
  void CursorSetupAccessors(Local<FunctionTemplate> tmpl);
  Local<Object> CursorGetNew(WT_CURSOR* cursor);
  Local<Object> CursorGetNew(
    WiredTigerSession *session,
    char* uri,
    char* config
  );
}
#endif
