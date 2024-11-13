#include <node.h>
#include "v8-version.h"
#include "../wiredtiger/helpers.h"

using namespace std;
using namespace v8;
using namespace node;

#ifndef NODE_HELPERS_H
#define NODE_HELPERS_H

#define THROW(x, m) return wiredtiger::binding::Return(wiredtiger::binding::ThrowException(Isolate::GetCurrent(), x(wiredtiger::binding::NewLatin1String(Isolate::GetCurrent(), m))), args)
#define THROW_RETURN(x, m, i) wiredtiger::binding::Return(wiredtiger::binding::ThrowException(Isolate::GetCurrent(), x(wiredtiger::binding::NewLatin1String(Isolate::GetCurrent(), m))), args); return i

namespace wiredtiger::binding {
  template<class T>
  struct PersistentClassPair {
    Global<Object> persistent;
    T* actual;
  };

  template <class T>
  void WeakCleanupCallback(const v8::WeakCallbackInfo<PersistentClassPair<T>>& data){
    PersistentClassPair<T>* pair = data.GetParameter();
    pair->persistent.Reset(Isolate::GetCurrent(), v8::Local<v8::Object>{});
    delete pair->actual;
    delete pair;
  }

  template <class T>
  void BindClassToV8(Isolate* isolate, Local<Object> handle, T* actual) {
    PersistentClassPair<T>* pair = new PersistentClassPair<T>();
    pair->actual = actual;
    pair->persistent.Reset(isolate, handle);
    pair->persistent.SetWeak(pair, WeakCleanupCallback<T>, v8::WeakCallbackType::kParameter);
  }

  template<class T>
  // TODO why ref and not pointer?
  T& Unwrap(Local<Object> handle) {
    assert(!handle.IsEmpty());
    assert(handle->InternalFieldCount() == 1);
    return *static_cast<T*>(handle->GetAlignedPointerFromInternalField(0));
  }


  template <class T>
  void Return(Local<T> handle, const FunctionCallbackInfo<Value>& args) {
    args.GetReturnValue().Set(handle);
  }
  template <class T>
  void Return(Local<T> handle, PropertyCallbackInfo<Value> info) {
    info.GetReturnValue().Set(handle);
  }
  template <class T>
  void Return(Persistent<T>& handle, PropertyCallbackInfo<Value> info) {
    info.GetReturnValue().Set(Local<T>::New(Isolate::GetCurrent(), handle));
  }

  template <class T>
  Local<T> Deref(Isolate* isolate, Persistent<T>& handle) {
    return Local<T>::New(isolate, handle);
  }

  Local<String> NewLatin1String(Isolate* isolate, const char* string, int length);
  Local<String> NewLatin1String(Isolate* isolate, const char* string);

  Local<Value> ThrowException(Isolate* isolate, Local<Value> exception);
  Local<FunctionTemplate> NewFunctionTemplate(
    Isolate* isolate,
    FunctionCallback callback,
    Local<Value> data = Local<Value>(),
    Local<Signature> signature = Local<Signature>(),
    int length = 0
  );


  void ThrowExtractError(int result, const FunctionCallbackInfo<Value>& args);


  Local<Signature> NewSignature(
    Isolate* isolate,
    Local<FunctionTemplate> receiver = Local<FunctionTemplate>()
  );

  template <class T>
  void Reset(Isolate* isolate, Persistent<T>& persistent, Local<T> handle) {
    persistent.Reset(isolate, handle);
  }

  void StringToCharPointer(Isolate* isolate, Local<String> string, char* where);

  char* ArgToCharPointer (Isolate* isolate, Local<Value> value);
  int populateArray(
    Isolate* isolate,
    Local<Array> items,
    QueryValue* values,
    size_t size
  );
  int populateArray(
    Isolate* isolate,
    Local<Array> items,
    std::vector<QueryValue>* values
  );
  int populateArrayItem(
    Isolate* isolate,
    Local<Array> items,
    int v,
    QueryValueValue value,
    char format,
    size_t size
  );
  int extractValue(
    Local<Value> val,
    Isolate* isolate,
    // we're never going to extract a WT item (I don't think? Raw mode?) at least for now this saves refactoring this complex method
    QueryValue* converted,
    Format format
  );
  int extractValues(
    Local<Array> values,
    Isolate* isolate,
    Local<Context> context,
    std::vector<QueryValue> *convertedValues,
    std::vector<Format> *formats
  );
  int parseConditions(
    Isolate* isolate,
    Local<Context> context,
    Local<Array> *conditionSpecsPointer,
    std::vector<QueryCondition> *conditions,
    std::vector<Format> *formats
  );

}
#endif
