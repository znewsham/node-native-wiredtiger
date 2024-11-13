#include <node.h>
#include <nan.h>
#include "v8-version.h"
#include "../wiredtiger/custom_extractor.h"
#include "../helpers.h"
#include "./helpers.h"
#ifndef NODE_CUSTOM_EXTRACTOR_H
#define NODE_CUSTOM_EXTRACTOR_H
namespace wiredtiger::binding {
  class WrappedCustomExtractor: public CustomExtractor {
    private:
      v8::Global<v8::Object> self;
      v8::Global<v8::Function> extractCb2;
      Nan::Callback extractCb;
      Nan::Callback customizeCb;
      Nan::Callback terminateCb;
    public:

      WrappedCustomExtractor(
        Local<Object> _self,
        Local<Function> extract,
        Local<Function> customize,
        Local<Function> terminate
      );

      int extract(
        WT_SESSION *session,
        const WT_ITEM *key,
        const WT_ITEM *value,
        WT_CURSOR *result_cursor
      );

      int customize(
        WT_SESSION *session,
        const char *uri,
        WT_CONFIG_ITEM *appcfg,
        WT_EXTRACTOR **customp
      );

      int terminate(
        WT_SESSION* session
      );
  };

  void CustomExtractorInit(Local<Object> target);
}

#endif
