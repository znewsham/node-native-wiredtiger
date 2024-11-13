#include "v8-version.h"
#include "../helpers.h"
#include "helpers.h"
#include "../wiredtiger/cursor.h"
#include <node.h>
#include <node_buffer.h>
#include <nan.h>
#include <v8.h>
#include <avcall.h>
#include "../wiredtiger/types.h"
#include "session.h"

namespace wiredtiger::binding {
  Persistent<FunctionTemplate> CursorConstructorTmpl;

  void CursorReset(const FunctionCallbackInfo<Value>& args) {
    Cursor& that = Unwrap<Cursor>(args.Holder());
    int result = that.reset();
    if (result != 0) {
      THROW(Exception::TypeError, wiredtiger_strerror(result));
    }
  }

  void CursorClose(const FunctionCallbackInfo<Value>& args) {
    Cursor& that = Unwrap<Cursor>(args.Holder());
    int result = that.close();
    if (result != 0) {
      THROW(Exception::TypeError, wiredtiger_strerror(result));
    }
  }

  void CursorNext(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = Isolate::GetCurrent();
    Cursor& that = Unwrap<Cursor>(args.Holder());
    int result = that.next();
    if (result != 0 && result != WT_NOTFOUND) {
      THROW(Exception::TypeError, wiredtiger_strerror(result));
    }
    Return(Boolean::New(isolate, result == 0 ? true : false), args);
  }

  void CursorPrev(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = Isolate::GetCurrent();
    Cursor& that = Unwrap<Cursor>(args.Holder());
    int result = that.prev();
    if (result != 0 && result != WT_NOTFOUND) {
      THROW(Exception::TypeError, wiredtiger_strerror(result));
    }
    Return(Boolean::New(isolate, result == 0 ? true : false), args);
  }

  void CursorGetKey(const FunctionCallbackInfo<Value>& args) {
    // we shouldn't be doing this - it assumes raw.
    Isolate* isolate = Isolate::GetCurrent();
    Cursor& that = Unwrap<Cursor>(args.Holder());
    if (args.Length() > 1 || (args.Length() == 1 && !args[0]->IsString())) {
      THROW(Exception::TypeError, "If an argument is provided it should be the format of the return value as a string");
    }

    std::vector<Format> requestedFormat;
    bool didRequestFormat = false;
    int result = 0;
    if (args.Length() == 1) {
      char* requestedFormatStr = ArgToCharPointer(isolate, args[0]);
      didRequestFormat = true;
      result = parseFormat(requestedFormatStr, &requestedFormat);
      free(requestedFormatStr);
    }
    if (didRequestFormat && requestedFormat.size() != that.columnCount(false)) {
      THROW(Exception::TypeError, "Incorrect column length");
    }

    size_t size = that.columnCount(false);
    std::vector<QueryValue> valueArray(size);
    result = that.getKey(&valueArray); // after this point everything in valueArray is a QueryValue
    Local<Array> retArray = Array::New(isolate, size);
    if (result == INVALID_VALUE_TYPE) {
      THROW(Exception::TypeError, "Invalid type encountered");
    }
    for (size_t i = 0; i < size; i++) {
      QueryValue* item = &valueArray[i];
      Format format = !didRequestFormat ? that.formatAt(false, i) : requestedFormat.at(i);
      result = populateArrayItem(isolate, retArray, i, item->value, format.format, item->size);
      if (result == INVALID_VALUE_TYPE) {
        THROW(Exception::TypeError, "Invalid type encountered");
      }
    }
    Return(retArray, args);
  }

  void CursorGetValue(const FunctionCallbackInfo<Value>& args) {
    // we shouldn't be doing this - it assumes raw.
    Isolate* isolate = Isolate::GetCurrent();
    Cursor& that = Unwrap<Cursor>(args.Holder());
    if (args.Length() > 1 || (args.Length() == 1 && !args[0]->IsString())) {
      THROW(Exception::TypeError, "If an argument is provided it should be the format of the return value as a string");
    }

    std::vector<Format> requestedFormat;
    bool didRequestFormat = false;
    int result = 0;
    if (args.Length() == 1) {
      char* requestedFormatStr = ArgToCharPointer(isolate, args[0]);
      didRequestFormat = true;
      result = parseFormat(requestedFormatStr, &requestedFormat);
      free(requestedFormatStr);
    }
    size_t size = that.columnCount(true);
    std::vector<QueryValue> valueArray(size);
    if (didRequestFormat && requestedFormat.size() != that.columnCount(true)) {
      THROW(Exception::TypeError, "Incorrect column length");
    }
    result = that.getValue(&valueArray); // after this point everything in valueArray is a QueryValue
    if (result == INVALID_VALUE_TYPE) {
      THROW(Exception::TypeError, "Invalid type encountered");
      // free(valueArray);
      return;
    }
    Local<Array> retArray = Array::New(isolate, size);
    for (size_t i = 0; i < size; i++) {
      QueryValue* item = &valueArray[i];
      Format format = !didRequestFormat ? that.formatAt(true, i) : requestedFormat.at(i);
      result = populateArrayItem(isolate, retArray, i, item->value, format.format, item->size);
      if (result == INVALID_VALUE_TYPE) {
        THROW(Exception::TypeError, "Invalid type encountered");
      // free(valueArray);
        return;
      }
    }
    Return(retArray, args);
    // freeQueryValues(valueArray, size);
  }
  void CursorSetKey(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = Isolate::GetCurrent();
    Cursor& that = Unwrap<Cursor>(args.Holder());

    std::vector<QueryValue>* values = new std::vector<QueryValue>(args.Length());
    for (int i = 0; i < args.Length(); i++) {
      int error = extractValue(
        args[i],
        isolate,
        &(*values)[i],
        that.formatAt(false, i)
      );
      if (error != 0) {
        ThrowExtractError(error, args);
        return;
      }
    }
    // TODO: this can't be freed until after the cursor executes (e.g., insert/update/search/searchNear)
    that.setKey(values);
  }

  void CursorSetValue(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = Isolate::GetCurrent();
    Cursor& that = Unwrap<Cursor>(args.Holder());

    std::vector<QueryValue> values(args.Length());
    for (int i = 0; i < args.Length(); i++) {
      int error = extractValue(
        args[i],
        isolate,
        &values[i],
        that.formatAt(true, i)
      );
      if (error != 0) {
        ThrowExtractError(error, args);
        return;
      }
    }
    that.setValue(&values);
  }
  void CursorSearch(const FunctionCallbackInfo<Value>& args) {
    Cursor& that = Unwrap<Cursor>(args.Holder());
    int result = that.search();
    if (result != 0 && result != WT_NOTFOUND) {
      THROW(Exception::TypeError, wiredtiger_strerror(result));
      return;
    }
    args.GetReturnValue().Set(result == WT_NOTFOUND ? false : true);
  }
  void CursorSearchNear(const FunctionCallbackInfo<Value>& args) {
    Cursor& that = Unwrap<Cursor>(args.Holder());
    int exact;
    int result = that.searchNear(&exact);
    if (result != 0) {
      THROW(Exception::TypeError, wiredtiger_strerror(result));
      return;
    }
    args.GetReturnValue().Set(exact);
  }

  void CursorInsert(const FunctionCallbackInfo<Value>& args) {
    Cursor& that = Unwrap<Cursor>(args.Holder());
    int result = that.insert();
    if (result) {
      THROW(Exception::TypeError, wiredtiger_strerror(result));
    }
  }
  void CursorUpdate(const FunctionCallbackInfo<Value>& args) {
    Cursor& that = Unwrap<Cursor>(args.Holder());
    int result = that.update();
    if (result != 0 && result != WT_NOTFOUND) {
      THROW(Exception::TypeError, wiredtiger_strerror(result));
    }
    Return(Boolean::New(Isolate::GetCurrent(), result == 0), args);
  }
  void CursorRemove(const FunctionCallbackInfo<Value>& args) {
    Cursor& that = Unwrap<Cursor>(args.Holder());
    int result = that.remove();
    if (result != 0 && result != WT_NOTFOUND) {
      THROW(Exception::TypeError, wiredtiger_strerror(result));
    }
    Return(Boolean::New(Isolate::GetCurrent(), result == 0), args);
  }
  void CursorBound(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = Isolate::GetCurrent();
    Cursor& that = Unwrap<Cursor>(args.Holder());
    if (args.Length() != 1 || !args[0]->IsString()) {
      THROW(Exception::TypeError, "Takes a config string");
    }
    char* config = ArgToCharPointer(isolate, args[0]);
    int result = that.bound(config);
    if (result) {
      THROW(Exception::TypeError, wiredtiger_strerror(result));
    }
  }

  void CursorCompare(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = Isolate::GetCurrent();
    Cursor& that = Unwrap<Cursor>(args.Holder());
    if (args.Length() != 1 || !args[0]->IsObject()) {
      THROW(Exception::TypeError, "Takes a cursor");
    }
    Cursor& other = Unwrap<Cursor>(args[0]->ToObject(isolate->GetCurrentContext()).ToLocalChecked());
    int compare;
    int result = that.compare(&other, &compare);
    if (result) {
      THROW(Exception::TypeError, wiredtiger_strerror(result));
      return;
    }
    args.GetReturnValue().Set(result);
  }

  void CursorEquals(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = Isolate::GetCurrent();
    Cursor& that = Unwrap<Cursor>(args.Holder());
    if (args.Length() != 1 || !args[0]->IsObject()) {
      THROW(Exception::TypeError, "Takes a cursor");
    }
    Cursor& other = Unwrap<Cursor>(args[0]->ToObject(isolate->GetCurrentContext()).ToLocalChecked());
    int compare;
    int result = that.equals(&other, &compare);
    if (result) {
      THROW(Exception::TypeError, wiredtiger_strerror(result));
      return;
    }
    args.GetReturnValue().Set(result);
  }

  void CursorGetRawKeyAndValue(const FunctionCallbackInfo<Value>& args) {
    // TODO: this mostly doesn't work, not sure why - not very important.
    // Isolate* isolate = Isolate::GetCurrent();
    // Cursor& that = Unwrap<Cursor>(args.Holder());
    // WT_ITEM key;
    // WT_ITEM value;
    // int result = that.get_value(&that, &key, &value);
    // if (result != 0) {
    //   printf("Errored?: %d\n", result);
    //   THROW(Exception::TypeError, that.session->strerror(that.session, result));
    // }

    // Local<Object> keyBuffer = Nan::CopyBuffer((char*)key.data, key.size).ToLocalChecked();
    // Local<Object> valueBuffer = Nan::CopyBuffer((char*)value.data, value.size).ToLocalChecked();
    // Local<Array>arr = Array::New(isolate, 2);
    // arr->Set(
    //   isolate->GetCurrentContext(),
    //   0,
    //   keyBuffer
    // );
    // arr->Set(
    //   isolate->GetCurrentContext(),
    //   1,
    //   valueBuffer
    // );
    // Return(arr, args);
  }

  void CursorAccessorSession(Local<String> property, const PropertyCallbackInfo<Value> &args) {
    Cursor& that = Unwrap<Cursor>(args.Holder());
    Return(WiredTigerSessionGetOrCreate(that.getRawSession()), args);
  }

  void CursorAccessorUri(Local<String> property, const PropertyCallbackInfo<Value> &args) {
    Isolate* isolate = Isolate::GetCurrent();
    Cursor& that = Unwrap<Cursor>(args.Holder());
    Return(NewLatin1String(isolate, that.getUri()), args);
  }

  void CursorAccessorKeyFormat(Local<String> property, const PropertyCallbackInfo<Value> &args) {
    Isolate* isolate = Isolate::GetCurrent();
    Cursor& that = Unwrap<Cursor>(args.Holder());
    Return(NewLatin1String(isolate, that.getKeyFormat()), args);
  }

  void CursorAccessorValueFormat(Local<String> property, const PropertyCallbackInfo<Value> &args) {
    Isolate* isolate = Isolate::GetCurrent();
    Cursor& that = Unwrap<Cursor>(args.Holder());
    Return(NewLatin1String(isolate, that.getValueFormat()), args);
  }

  void CursorGetterFlags(Local<String> property, const PropertyCallbackInfo<Value> &args) {
    Isolate* isolate = Isolate::GetCurrent();
    Cursor& that = Unwrap<Cursor>(args.Holder());
    Return(Uint32::New(isolate, that.getFlags()), args);
  }

  void CursorSetterFlags(Local<String> property, Local<Value> value, const PropertyCallbackInfo<void> &args) {
    Isolate* isolate = Isolate::GetCurrent();
    Cursor& that = Unwrap<Cursor>(args.Holder());
    uint32_t intVal = value->ToUint32(isolate->GetCurrentContext()).ToLocalChecked()->Value();
    that.setFlags(intVal);
  }

  void CursorNewInternal(const FunctionCallbackInfo<Value>& args) {
    Return(args.This(), args);
  }

  Local<Object> CursorGetNew(WT_CURSOR* wtCursor) {
    Isolate* isolate = Isolate::GetCurrent();
    Local<Function> fn = CursorConstructorTmpl.Get(isolate)->GetFunction(isolate->GetCurrentContext()).ToLocalChecked();
    auto x = fn->NewInstance(isolate->GetCurrentContext());
    Local<Object> obj = x.ToLocalChecked();
    Cursor* cursor = new Cursor(wtCursor);
    obj->SetAlignedPointerInInternalField(0, cursor);
    BindClassToV8<Cursor>(isolate, obj, cursor);
    return obj;
  }


  Local<Object> CursorGetNew(WiredTigerSession *session, char* uri, char* config) {
    Isolate* isolate = Isolate::GetCurrent();
    Local<Function> fn = Deref(isolate, CursorConstructorTmpl)->GetFunction(isolate->GetCurrentContext()).ToLocalChecked();
    Local<Object> obj = fn->NewInstance(isolate->GetCurrentContext()).ToLocalChecked();

    // TODO: handle the result
    Cursor* cursor;
    session->openCursor(uri, config, &cursor);
    cursor->setExternal();
    obj->SetAlignedPointerInInternalField(0, cursor);
    BindClassToV8<Cursor>(isolate, obj, cursor);
    return obj;
  }

  void CursorSetupAccessors(Local<FunctionTemplate> tmpl) {
    Isolate* isolate = Isolate::GetCurrent();
    tmpl->InstanceTemplate()->SetAccessor(NewLatin1String(isolate, "session"), CursorAccessorSession);
    tmpl->InstanceTemplate()->SetAccessor(NewLatin1String(isolate, "uri"), CursorAccessorUri);
    tmpl->InstanceTemplate()->SetAccessor(NewLatin1String(isolate, "keyFormat"), CursorAccessorKeyFormat);
    tmpl->InstanceTemplate()->SetAccessor(NewLatin1String(isolate, "valueVormat"), CursorAccessorValueFormat);
    tmpl->InstanceTemplate()->SetAccessor(NewLatin1String(isolate, "flags"), CursorGetterFlags, CursorSetterFlags);
  }

  void CursorInit(Local<Object> target) {
    Isolate* isolate = Isolate::GetCurrent();
    Local<FunctionTemplate> tmpl = NewFunctionTemplate(isolate, CursorNewInternal);
    CursorConstructorTmpl.Reset(isolate, tmpl);
    tmpl->SetClassName(NewLatin1String(isolate, "Cursor"));
    tmpl->InstanceTemplate()->SetInternalFieldCount(1);

    // Cursor.prototype
    Local<ObjectTemplate> proto = tmpl->PrototypeTemplate();
    Local<Signature> sig = Signature::New(isolate, tmpl);
    proto->Set(NewLatin1String(isolate, "reset"), NewFunctionTemplate(isolate, CursorReset, Local<Value>(), sig));
    proto->Set(NewLatin1String(isolate, "close"), NewFunctionTemplate(isolate, CursorClose, Local<Value>(), sig));
    proto->Set(NewLatin1String(isolate, "next"), NewFunctionTemplate(isolate, CursorNext, Local<Value>(), sig));
    proto->Set(NewLatin1String(isolate, "prev"), NewFunctionTemplate(isolate, CursorPrev, Local<Value>(), sig));
    proto->Set(NewLatin1String(isolate, "getKey"), NewFunctionTemplate(isolate, CursorGetKey, Local<Value>(), sig));
    proto->Set(NewLatin1String(isolate, "getValue"), NewFunctionTemplate(isolate, CursorGetValue, Local<Value>(), sig));
    proto->Set(NewLatin1String(isolate, "setKey"), NewFunctionTemplate(isolate, CursorSetKey, Local<Value>(), sig));
    proto->Set(NewLatin1String(isolate, "setValue"), NewFunctionTemplate(isolate, CursorSetValue, Local<Value>(), sig));
    proto->Set(NewLatin1String(isolate, "search"), NewFunctionTemplate(isolate, CursorSearch, Local<Value>(), sig));
    proto->Set(NewLatin1String(isolate, "searchNear"), NewFunctionTemplate(isolate, CursorSearchNear, Local<Value>(), sig));
    proto->Set(NewLatin1String(isolate, "insert"), NewFunctionTemplate(isolate, CursorInsert, Local<Value>(), sig));
    proto->Set(NewLatin1String(isolate, "update"), NewFunctionTemplate(isolate, CursorUpdate, Local<Value>(), sig));
    proto->Set(NewLatin1String(isolate, "remove"), NewFunctionTemplate(isolate, CursorRemove, Local<Value>(), sig));
    proto->Set(NewLatin1String(isolate, "bound"), NewFunctionTemplate(isolate, CursorBound, Local<Value>(), sig));
    proto->Set(NewLatin1String(isolate, "compare"), NewFunctionTemplate(isolate, CursorCompare, Local<Value>(), sig));
    proto->Set(NewLatin1String(isolate, "equals"), NewFunctionTemplate(isolate, CursorEquals, Local<Value>(), sig));
    proto->Set(NewLatin1String(isolate, "getRawKeyAndValue"), NewFunctionTemplate(isolate, CursorGetRawKeyAndValue, Local<Value>(), sig));

  }
}
