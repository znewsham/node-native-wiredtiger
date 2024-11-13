#include "v8-version.h"
#include "../wiredtiger/find_cursor.h"
#include "../wiredtiger/session.h"
#include "../helpers.h"
#include "helpers.h"
#include "cursor.h"
#include "session.h"
#include "../wiredtiger/types.h"

namespace wiredtiger::binding {
  Persistent<FunctionTemplate> FindCursorConstructorTmpl;

  void FindCursorExtractFindOptions(Local<Object> optionsIn, FindOptions* optionsOut) {
    Isolate* isolate = Isolate::GetCurrent();
    Local<Context> context = isolate->GetCurrentContext();
    Local<String> keyFormatProp = NewLatin1String(isolate, "keyFormat");
    Local<String> valueFormatProp = NewLatin1String(isolate, "valueFormat");
    Local<String> sessionProp = NewLatin1String(isolate, "session");
    Local<String> columnsProp = NewLatin1String(isolate, "columns");
    if (optionsIn->Has(context, keyFormatProp).ToChecked()) {
      Local<String> keyFormat = Local<String>::Cast(optionsIn->Get(context, keyFormatProp).ToLocalChecked());
      optionsOut->keyFormat = (char*)calloc(sizeof(char), keyFormat->Utf8Length(isolate) + 1); // freed from the table find call
      StringToCharPointer(isolate, keyFormat, optionsOut->keyFormat);
    }
    if (optionsIn->Has(context, valueFormatProp).ToChecked()) {
      Local<String> valueFormat = Local<String>::Cast(optionsIn->Get(context, valueFormatProp).ToLocalChecked());
      optionsOut->valueFormat = (char*)calloc(sizeof(char), valueFormat->Utf8Length(isolate) + 1); // freed from the table find call
      StringToCharPointer(isolate, valueFormat, optionsOut->valueFormat);
    }
    if (optionsIn->Has(context, columnsProp).ToChecked()) {
      Local<String> columns = Local<String>::Cast(optionsIn->Get(context, columnsProp).ToLocalChecked());
      optionsOut->columns = (char*)calloc(sizeof(char), columns->Utf8Length(isolate) + 1); // freed from the table find call
      StringToCharPointer(isolate, columns, optionsOut->columns);
    }
    if (optionsIn->Has(context, sessionProp).ToChecked()) {
      Local<Object> sessionHolder = Local<Object>::Cast(optionsIn->Get(context, sessionProp).ToLocalChecked());
      WiredTigerSession session = Unwrap<WiredTigerSession>(sessionHolder);
      optionsOut->session = static_cast<WiredTigerSession*>(sessionHolder->GetAlignedPointerFromInternalField(0));
    }
  }

  void FindCursorReset(const FunctionCallbackInfo<Value>& args) {
    FindCursor& that = Unwrap<FindCursor>(args.Holder());
    that.reset();
  }

  void FindCursorNextBatch(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = Isolate::GetCurrent();
    FindCursor& that = Unwrap<FindCursor>(args.Holder());
    int batchSize = 100;
    if (args.Length() >= 1 && !args[0]->IsNumber() && !args[0]->IsUndefined()) {
      THROW(Exception::TypeError, "A provided batch size must be numeric");
    }
    else if (args.Length() >= 1 && args[0]->IsNumber()) {
      Local<Integer> batchSizeInt = Local<Integer>::Cast(args[0]);
      batchSize = batchSizeInt->Value();
    }
    std::vector<std::unique_ptr<EntryOfVectors>> results;
    int result = that.nextBatch(batchSize, &results);

    int resultSize = 0;
    if (result != 0) {
      if (result >= ERROR_BASE) {
        printf("Error: %02x\n", result);
        THROW(Exception::TypeError, "Usage error");// TODO map error codes
      }
      if (result != WT_NOTFOUND) {
        THROW(Exception::TypeError, that.session->strerror(result));
        // freeResults(&results);
        return;
      }
    }
    resultSize = results.size();

    Local<Array>arr = Array::New(isolate, resultSize);
    for (int i = 0; i < resultSize; i++) {
      EntryOfVectors& entry = *results.at(i);
      Local<Array>keyValues = Array::New(isolate, entry.keyArray->size());
      Local<Array>valueValues = Array::New(isolate, entry.valueArray->size());
      populateArray(isolate, keyValues, entry.keyArray);
      populateArray(isolate, valueValues, entry.valueArray);
      Local<Array>entryArray = Array::New(isolate, 2);
      entryArray->Set(
        isolate->GetCurrentContext(),
        0,
        keyValues
      ).Check();
      entryArray->Set(
        isolate->GetCurrentContext(),
        1,
        valueValues
      ).Check();
      arr->Set(
        isolate->GetCurrentContext(),
        i,
        entryArray
      ).Check();
    }
    Return(arr, args);
  }

  void FindCursorClose(const FunctionCallbackInfo<Value>& args) {
    FindCursor& that = Unwrap<FindCursor>(args.Holder());
    int result = that.close();
    if (result != 0) {
      THROW(Exception::TypeError, that.session->strerror(result));
    }
  }

  void FindCursorCount(const FunctionCallbackInfo<Value>& args) {
    FindCursor& that = Unwrap<FindCursor>(args.Holder());
    int counter;
    int result = that.count(&counter);
    if (result != 0) {
      THROW(Exception::TypeError, that.session->strerror(result));
    }
    Return(Int32::New(Isolate::GetCurrent(), counter), args);
  }

  void FindCursorNewInternal(const FunctionCallbackInfo<Value>& args) {
  }

  Local<Object> FindCursorGetNew(char* tableName, std::vector<QueryCondition>* conditions, FindOptions* options) {
    Isolate* isolate = Isolate::GetCurrent();
    Local<Function> fn = Deref(isolate, FindCursorConstructorTmpl)->GetFunction(isolate->GetCurrentContext()).ToLocalChecked();
    Local<Object> obj = fn->NewInstance(isolate->GetCurrentContext()).ToLocalChecked();

    FindCursor* sc = new FindCursor(tableName, conditions, options); // deleted on close
    sc->setExternal();
    obj->SetAlignedPointerInInternalField(0, sc);
    return obj;
  }

  void FindCursorInit(Local<Object> target) {
    Isolate* isolate = Isolate::GetCurrent();
    Local<FunctionTemplate> tmpl = NewFunctionTemplate(isolate, FindCursorNewInternal);
    FindCursorConstructorTmpl.Reset(isolate, tmpl);
    tmpl->SetClassName(NewLatin1String(isolate, "FindCursor"));
    tmpl->InstanceTemplate()->SetInternalFieldCount(1);
    CursorSetupAccessors(tmpl);

    // FindCursor.prototype
    Local<ObjectTemplate> proto = tmpl->PrototypeTemplate();
    Local<Signature> sig = Signature::New(isolate, tmpl);
    proto->Set(NewLatin1String(isolate, "reset"), NewFunctionTemplate(isolate, FindCursorReset, Local<Value>(), sig));
    proto->Set(NewLatin1String(isolate, "nextBatch"), NewFunctionTemplate(isolate, FindCursorNextBatch, Local<Value>(), sig));
    proto->Set(NewLatin1String(isolate, "close"), NewFunctionTemplate(isolate, FindCursorClose, Local<Value>(), sig));
    proto->Set(NewLatin1String(isolate, "count"), NewFunctionTemplate(isolate, FindCursorCount, Local<Value>(), sig));
    // proto->Set(NewLatin1String(isolate, "metrics"), NewFunctionTemplate(isolate, FindCursorMetrics, Local<Value>(), sig));
  }
}
