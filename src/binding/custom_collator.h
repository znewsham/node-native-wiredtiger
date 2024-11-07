#include <node.h>
#include <nan.h>
#include "v8-version.h"
#include "../wiredtiger/custom_collator.h"
#include "../helpers.h"
#include "helpers.h"
#ifndef NODE_CUSTOM_COLLATOR_H
#define NODE_CUSTOM_COLLATOR_H
namespace wiredtiger::binding {
  class WrappedCustomCollator: public CustomCollator {
    private:
      v8::Global<v8::Object> self;
      Nan::Callback compareCb;
      Nan::Callback customizeCb;
      Nan::Callback terminateCb;
    public:
      WrappedCustomCollator(
        Local<Object> _self,
        Local<Function> compare,
        Local<Function> customize,
        Local<Function> terminate
      );

      int compare(
        WT_SESSION* session,
        const WT_ITEM* v1,
        const WT_ITEM* v2,
        int* cmp
      );

      int customize(
        WT_SESSION* session,
        const char* uri,
        WT_CONFIG_ITEM* appcfg,
        WT_COLLATOR** customp
      );

      int terminate(
        WT_SESSION* session
      );
  };

  void CustomCollatorInit(Local<Object> target);
}

#endif
