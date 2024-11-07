#include "v8-version.h"
#include "../wiredtiger/map_table.h"
#include "../helpers.h"
#include "map_table.h"
#include "helpers.h"
#include "../wiredtiger/types.h"


namespace wiredtiger::binding {
  Persistent<FunctionTemplate> WiredTigerMapTableConstructorTmpl;
  void WiredTigerMapTableList(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = Isolate::GetCurrent();
    WiredTigerMapTable& that = Unwrap<WiredTigerMapTable>(args.Holder());
    if (args.Length() >= 1) {
      if (!args[0]->IsString() && !args[0]->IsUndefined()) {
        THROW(Exception::TypeError, "If provided the keyPrefix must be a string");
        return;
      }
    }
    if (args.Length() >= 2) {
      if (!args[2]->IsNumber()) {
        THROW(Exception::TypeError, "If provided the limit must be a number");
        return;
      }
    }
    char* keyPrefix = NULL;
    int limit = -1;
    if (args.Length() >= 2 && args[0]->IsString()) {
      keyPrefix = ArgToCharPointer(isolate, args[0]);
    }
    if (args.Length() >= 3 && args[1]->IsNumber()) {
      limit = (int)Local<Number>::Cast(args[1])->Value();
    }
    std::vector<std::string> result;
    int error = that.list(keyPrefix, limit, &result);
    if (keyPrefix != NULL) {
      free(keyPrefix);
    }
    if (error) {
      if (error != WT_NOTFOUND) {
        THROW(Exception::TypeError, wiredtiger_strerror(error));
        return;
      }
    }
    Local<Array>arr = Array::New(isolate, result.size());
    for (size_t i = 0; i < result.size(); i++) {
      arr->Set(isolate->GetCurrentContext(), i, NewLatin1String(isolate, (char*)result.at(i).c_str())).Check();
    }
    Return(arr, args);
    // Return(Buffer::New(isolate, (char*)result.value.buffer, result.value.size).ToLocalChecked(), args);
  }

  void WiredTigerMapTableDel(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = Isolate::GetCurrent();
    WiredTigerMapTable& that = Unwrap<WiredTigerMapTable>(args.Holder());
    if (args.Length() != 1|| !args[0]->IsString()) {
      THROW(Exception::TypeError, "Must take a string argument");
    }
    char* key = ArgToCharPointer(isolate, args[0]);
    int result = that.del(key);
    free(key);
    if (result) {
      if (result == WT_NOTFOUND) {
        Return(Integer::New(isolate, 0), args);
      }
      THROW(Exception::TypeError, wiredtiger_strerror(result));
      return;
    }
    Return(Integer::New(isolate, result), args);
  }

  void WiredTigerMapTableSet(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = Isolate::GetCurrent();
    WiredTigerMapTable& that = Unwrap<WiredTigerMapTable>(args.Holder());
    if (args.Length() != 2 || !args[0]->IsString() || !args[1]->IsString()) {
      THROW(Exception::TypeError, "Must take two string arguments");
      return;
    }
    char* key = ArgToCharPointer(isolate, args[0]);
    char* value = ArgToCharPointer(isolate, args[1]);
    int result = that.set(key, value);
    free(key);
    free(value);
    if (result != 0) {
      THROW(Exception::TypeError, wiredtiger_strerror(result));
      return;
    }
    Return(Integer::New(isolate, result), args);
  }

  void WiredTigerMapTableGet(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = Isolate::GetCurrent();
    WiredTigerMapTable& that = Unwrap<WiredTigerMapTable>(args.Holder());
    if (args.Length() != 1 || !args[0]->IsString()) {
      THROW(Exception::TypeError, "Must take a string argument");
    }
    char* key = ArgToCharPointer(isolate, args[0]);
    char* value = NULL;
    int error = that.get(key, &value);
    free(key);
    if (error) {
      if (error == WT_NOTFOUND) {
        Return(Undefined(isolate), args);
        return;
      }
      THROW(Exception::TypeError, wiredtiger_strerror(error));
      return;
    }
    Return(NewLatin1String(isolate, value), args);
  }


  void WiredTigerMapTableNew(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = Isolate::GetCurrent();
    if (args.Length() != 2 || !args[0]->IsObject() || !args[1]->IsString()) {
      THROW(Exception::TypeError, "Must take a DB and string argument");
      return;
    }
    WiredTigerDB& that = Unwrap<WiredTigerDB>(Local<Object>::Cast(args[0]));
    char* tableName = ArgToCharPointer(isolate, args[1]);
    Local<Object> handle = args.This();
    WiredTigerMapTable* table = new WiredTigerMapTable(&that, tableName);
    handle->SetAlignedPointerInInternalField(0, table);
    Return(handle, args);
    free(tableName);
  }

  void WiredTigerMapTableInit(Local<Object> target) {
    Isolate* isolate = Isolate::GetCurrent();
    Local<Context> context = isolate->GetCurrentContext();

    Local<FunctionTemplate> tmpl = NewFunctionTemplate(isolate, WiredTigerMapTableNew);
    WiredTigerMapTableConstructorTmpl.Reset(isolate, tmpl);
    tmpl->SetClassName(NewLatin1String(isolate, "WiredTigerMapTable"));
    tmpl->InstanceTemplate()->SetInternalFieldCount(1);

    // WiredTigerMapTable.prototype
    Local<ObjectTemplate> proto = tmpl->PrototypeTemplate();
    Local<Signature> sig = Signature::New(isolate, tmpl);


    // at some point should probably separate out the raw session implementation from these helpers
    proto->Set(NewLatin1String(isolate, "get"), NewFunctionTemplate(isolate, WiredTigerMapTableGet, Local<Value>(), sig));
    proto->Set(NewLatin1String(isolate, "set"), NewFunctionTemplate(isolate, WiredTigerMapTableSet, Local<Value>(), sig));
    proto->Set(NewLatin1String(isolate, "del"), NewFunctionTemplate(isolate, WiredTigerMapTableDel, Local<Value>(), sig));
    proto->Set(NewLatin1String(isolate, "list"), NewFunctionTemplate(isolate, WiredTigerMapTableList, Local<Value>(), sig));
    Local<Function> fn = tmpl->GetFunction(Isolate::GetCurrent()->GetCurrentContext()).ToLocalChecked();
    target->Set(context, String::NewFromOneByte(isolate, (const uint8_t*)"WiredTigerMapTable", NewStringType::kNormal).ToLocalChecked(), fn).FromJust();
  }
}
