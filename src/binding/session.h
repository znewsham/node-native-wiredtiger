#include <node.h>
#include "v8-version.h"
#include "../wiredtiger/session.h"
#include "../helpers.h"
#include "helpers.h"
#ifndef NODE_SESSION_H
#define NODE_SESSION_H
namespace wiredtiger::binding {
  void WiredTigerSessionInit(Local<Object> target);
  void WiredTigerSessionRegister(WiredTigerSession* session);
  Local<Object> WiredTigerSessionGetOrCreate(WT_SESSION *session);
}
#endif
