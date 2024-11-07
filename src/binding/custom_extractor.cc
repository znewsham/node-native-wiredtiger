
#include <nan.h>
#include <node.h>
#include <v8-function.h>
#include <v8-local-handle.h>
#include "session.h"
#include "cursor.h"
#include "custom_extractor.h"
#include "../wiredtiger/custom_extractor.h"
#include "helpers.h"
#include "cfg_item.h"

namespace wiredtiger::binding {
  Persistent<FunctionTemplate> CustomExtractorConstructorTmpl;
  WrappedCustomExtractor::WrappedCustomExtractor(
    Local<Object> _self,
    Local<Function> extract,
    Local<Function> customize,
    Local<Function> terminate
  ): CustomExtractor() {
    self.Reset(Isolate::GetCurrent(), _self);
    extractCb.Reset(extract);
    hasCustomize = !customize.IsEmpty();
    hasTerminate = !terminate.IsEmpty();
    this->extractor.extract = CustomExtractor::Extract;
    if (!customize.IsEmpty()) {
      customizeCb.Reset(customize);
      this->extractor.customize = CustomExtractor::Customize;
    }
    if (!terminate.IsEmpty()) {
      terminateCb.Reset(terminate);
      this->extractor.terminate = CustomExtractor::Terminate;
    }
  }

  int WrappedCustomExtractor::extract(
    WT_SESSION* session,
    const WT_ITEM* key,
    const WT_ITEM* value,
    WT_CURSOR* result_cursor
  ) {
    Isolate* isolate = Isolate::GetCurrent();
    const unsigned argc = 4;
    Local<Value> argv[argc] = {
      WiredTigerSessionGetOrCreate(session),
      Nan::CopyBuffer((char*)key->data, key->size).ToLocalChecked(),
      Nan::CopyBuffer((char*)value->data, value->size).ToLocalChecked(),
      CursorGetNew(result_cursor),
    };
    Nan::Call(extractCb, self.Get(isolate), argc, argv);
    return 0;
  }

  int WrappedCustomExtractor::customize(
    WT_SESSION* session,
    const char* uri,
    WT_CONFIG_ITEM* appcfg,
    WT_EXTRACTOR** customp
  ) {
    Isolate* isolate = Isolate::GetCurrent();
    const unsigned argc = 3;
    Local<Value> argv[argc] = {
      WiredTigerSessionGetOrCreate(session),
      NewLatin1String(v8::Isolate::GetCurrent(), uri),
      GetConfigItemValue(appcfg)
    };

    Local<Value> retVal = Nan::Call(customizeCb, self.Get(isolate), argc, argv).ToLocalChecked();
    if (retVal.IsEmpty()) {
      return -1;
    }
    else if (!retVal->IsUndefined() && retVal->IsObject()) {
      CustomExtractor& extractor = Unwrap<CustomExtractor>(retVal->ToObject(isolate->GetCurrentContext()).ToLocalChecked());
      *customp = &extractor.extractor;
    }
    return 0;
  }

  int WrappedCustomExtractor::terminate(
    WT_SESSION* session
  ) {
    Isolate* isolate = Isolate::GetCurrent();
    const unsigned argc = 1;

    Local<Value> argv[argc] = {
      WiredTigerSessionGetOrCreate(session)
    };

    Nan::Call(terminateCb, self.Get(isolate), argc, argv);
    return 0;
  }

  void CustomExtractorNewExternal(const FunctionCallbackInfo<Value>& args) {
    Local<Object> handle = args.This();
    if (
      args.Length() < 1
      || !args[0]->IsFunction()
      || (args.Length() > 1 && !args[1]->IsFunction() && !args[1]->IsUndefined())
      || (args.Length() > 2 && !args[2]->IsFunction() && !args[2]->IsUndefined())
    ) {
      THROW(Exception::TypeError, "Takes fn, fn, fn");
    }
    Local<Function> extracterCb = Local<Function>::Cast(args[0]);
    Local<Function> customizerCb = args.Length() > 1 ? Local<Function>::Cast(args[1]) : Local<Function>();
    Local<Function> terminaterCb = args.Length() > 2 ? Local<Function>::Cast(args[2]) : Local<Function>();
    WrappedCustomExtractor* extractor = new WrappedCustomExtractor(handle, extracterCb, customizerCb, terminaterCb);
    handle->SetAlignedPointerInInternalField(0, extractor);
    Return(handle, args);
  }

  void CustomExtractorInit(Local<Object> target) {
    Isolate* isolate = Isolate::GetCurrent();
    Local<FunctionTemplate> tmpl = NewFunctionTemplate(isolate, CustomExtractorNewExternal);
    CustomExtractorConstructorTmpl.Reset(isolate, tmpl);
    tmpl->SetClassName(NewLatin1String(isolate, "CustomExtractor"));
    tmpl->InstanceTemplate()->SetInternalFieldCount(1);


    Local<Function> fn = tmpl->GetFunction(Isolate::GetCurrent()->GetCurrentContext()).ToLocalChecked();

    target->Set(isolate->GetCurrentContext(), String::NewFromOneByte(isolate, (const uint8_t*)"CustomExtractor", NewStringType::kNormal).ToLocalChecked(), fn).FromJust();
  }
}
