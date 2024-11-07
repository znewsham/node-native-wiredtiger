
#include <nan.h>
#include <node.h>
#include <v8-function.h>
#include <v8-local-handle.h>
#include "session.h"
#include "cursor.h"
#include "custom_collator.h"
#include "cfg_item.h"
#include "../wiredtiger/custom_collator.h"

namespace wiredtiger::binding {
  Persistent<FunctionTemplate> CustomCollatorConstructorTmpl;
  WrappedCustomCollator::WrappedCustomCollator(
    Local<Object> _self,
    Local<Function> compare,
    Local<Function> customize,
    Local<Function> terminate
  ): CustomCollator() {
    self.Reset(Isolate::GetCurrent(), _self);
    compareCb.Reset(compare);
    hasCustomize = !customize.IsEmpty();
    hasTerminate = !terminate.IsEmpty();
    if (!customize.IsEmpty()) {
      customizeCb.Reset(customize);
    }
    if (!terminate.IsEmpty()) {
      terminateCb.Reset(terminate);
    }
  }

  int WrappedCustomCollator::compare(
    WT_SESSION* session,
    const WT_ITEM* v1,
    const WT_ITEM* v2,
    int* cmp
  ) {
    Isolate* isolate = Isolate::GetCurrent();
    const unsigned argc = 3;
    Local<Value> argv[argc] = {
      WiredTigerSessionGetOrCreate(session),
      Nan::CopyBuffer((char*)v1->data, v1->size).ToLocalChecked(),
      Nan::CopyBuffer((char*)v2->data, v2->size).ToLocalChecked(),
    };

    Local<Value> retVal = compareCb.Call(self.Get(isolate), argc, argv);
    if (retVal->IsInt32()) {
      *cmp = retVal->ToInt32(Isolate::GetCurrent()->GetCurrentContext()).ToLocalChecked()->Value();
    }
    return 0;
  }

  int WrappedCustomCollator::customize(
    WT_SESSION* session,
    const char* uri,
    WT_CONFIG_ITEM* appcfg,
    WT_COLLATOR** customp
  ) {
    Isolate* isolate = Isolate::GetCurrent();
    const unsigned argc = 3;

    Local<Value> argv[argc] = {
      WiredTigerSessionGetOrCreate(session),
      NewLatin1String(v8::Isolate::GetCurrent(), uri),
      GetConfigItemValue(appcfg)
    };

    Local<Value> retVal = customizeCb.Call(self.Get(isolate), argc, argv);
    if (retVal.IsEmpty()) {
      return -1;
    }
    else if (!retVal->IsUndefined() && retVal->IsObject()) {
      CustomCollator& collator = Unwrap<CustomCollator>(retVal->ToObject(isolate->GetCurrentContext()).ToLocalChecked());
      *customp = &collator.collator;
    }
    return 0;
  }

  int WrappedCustomCollator::terminate(
    WT_SESSION* session
  ) {
    Isolate* isolate = Isolate::GetCurrent();
    const unsigned argc = 1;

    Local<Value> argv[argc] = {
      WiredTigerSessionGetOrCreate(session)
    };

    terminateCb.Call(self.Get(isolate), argc, argv);
    return 0;
  }

  void CustomCollatorNewExternal(const FunctionCallbackInfo<Value>& args) {
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
    WrappedCustomCollator* extractor = new WrappedCustomCollator(handle, extracterCb, customizerCb, terminaterCb);
    handle->SetAlignedPointerInInternalField(0, extractor);
    Return(handle, args);
  }

  void CustomCollatorInit(Local<Object> target) {
    Isolate* isolate = Isolate::GetCurrent();
    Local<FunctionTemplate> tmpl = NewFunctionTemplate(isolate, CustomCollatorNewExternal);
    CustomCollatorConstructorTmpl.Reset(isolate, tmpl);
    tmpl->SetClassName(NewLatin1String(isolate, "CustomCollator"));
    tmpl->InstanceTemplate()->SetInternalFieldCount(1);

    Local<Function> fn = tmpl->GetFunction(Isolate::GetCurrent()->GetCurrentContext()).ToLocalChecked();

    target->Set(isolate->GetCurrentContext(), String::NewFromOneByte(isolate, (const uint8_t*)"CustomCollator", NewStringType::kNormal).ToLocalChecked(), fn).FromJust();
  }
}
