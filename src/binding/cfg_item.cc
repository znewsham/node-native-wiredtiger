#include <node.h>
#include "cfg_item.h"
#include "v8-local-handle.h"
#include "v8-value.h"
#include "helpers.h"
#include <wiredtiger.h>

using namespace v8;
namespace wiredtiger::binding {
  Local<Value> GetConfigItemValue(WT_CONFIG_ITEM* appcfg) {
    Isolate* isolate = Isolate::GetCurrent();
    if (appcfg->type == WT_CONFIG_ITEM_TYPE::WT_CONFIG_ITEM_STRING) {
      return NewLatin1String(isolate, appcfg->str, appcfg->len);
    }
    if (appcfg->type == WT_CONFIG_ITEM_TYPE::WT_CONFIG_ITEM_BOOL) {
      return Boolean::New(isolate, appcfg->val);
    }
    if (appcfg->type == WT_CONFIG_ITEM_TYPE::WT_CONFIG_ITEM_NUM) {
      return Number::New(isolate, appcfg->val);
    }
  }
}
