#include <node.h>
#include <node_buffer.h>
#include <avcall.h>
#include "../wiredtiger/wt_item.h"
#include "wt_item.h"
#include "v8-local-handle.h"
#include "v8-value.h"
#include "helpers.h"
#include <wiredtiger.h>

using namespace v8;
namespace wiredtiger::binding {
  void WtConfigParserOpen(const FunctionCallbackInfo<Value>& args) {

  }
  void WtItemStructUnpack(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = Isolate::GetCurrent();
    if (args.Length() != 3 || !node::Buffer::HasInstance(args[1]) || !args[2]->IsString()) {
      THROW(Exception::TypeError, "Must provide a buffer and a format to extract");
    }
    // TODO: extract session
    WiredTigerSession& session = Unwrap<WiredTigerSession>(args[0]->ToObject(isolate->GetCurrentContext()).ToLocalChecked());
    char* format = ArgToCharPointer(isolate, args[2]);

    WT_ITEM item;
    item.data = node::Buffer::Data(args[1]);
    item.size = node::Buffer::Length(args[1]);

    std::vector<Format> formats;
    int error = parseFormat(format, &formats);
    if (error) {
      free(format);
      THROW(Exception::TypeError, "Invalid Format");
    }
    char* cleanedFormat;
    formatsToString(&formats, &cleanedFormat);

    std::vector<QueryValue> queryValues(formats.size());

    error = unpackWtItem(
      session.getWTSession(),
      &item,
      cleanedFormat,
      &formats,
      &queryValues
    );

    free(format);
    free(cleanedFormat);
    if (error) {
      THROW(Exception::TypeError, wiredtiger_strerror(error));
    }
    Local<Array> result = Array::New(isolate, queryValues.size());
    error = populateArray(isolate, result, &queryValues);
    if (error) {
      ThrowExtractError(error, args);
      return;
    }
    Return(result, args);
  }

  void WtItemInit(Local<Object> target) {
    Isolate* isolate = Isolate::GetCurrent();
    Local<Context> context = isolate->GetCurrentContext();
    NODE_SET_METHOD(target, "wiredTigerStructUnpack", WtItemStructUnpack);
    NODE_SET_METHOD(target, "wiredTigerConfigParserOpen", WtConfigParserOpen);

  }
}
