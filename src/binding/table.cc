#include "v8-version.h"
#include "../wiredtiger/db.h"
#include "../wiredtiger/table.h"
#include "../helpers.h"
#include "./helpers.h"
#include "find_cursor.h"
#include "../wiredtiger/types.h"
#include "session.h"

namespace wiredtiger::binding {
  Persistent<FunctionTemplate> WiredTigerTableConstructorTmpl;

  void WiredTigerTableNew(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = Isolate::GetCurrent();
    Local<Object> handle = args.This();
    if (args.Length() != 3 || !args[0]->IsObject() || !args[1]->IsString() || !args[2]->IsString()) {
      THROW(Exception::TypeError, "call signature is [db, tableName, tableConfig]");
    }
    WiredTigerDB& db = Unwrap<WiredTigerDB>(Local<Object>::Cast(args[0]));
    char* tableName = ArgToCharPointer(isolate, args[1]);
    char* config = ArgToCharPointer(isolate, args[2]);

    WiredTigerTable* table = new WiredTigerTable(&db, tableName, config);
    handle->SetAlignedPointerInInternalField(0, table);
    BindClassToV8<WiredTigerTable>(isolate, handle, table);
    Return(args.This(), args);
    free(tableName);
    free(config);
  }

  WiredTigerSession* GetSession(WiredTigerDB* db) {
    WT_SESSION* wtSession;
    db->conn->open_session(db->conn, NULL, NULL, &wtSession);
    WiredTigerSession* session = new WiredTigerSession(wtSession);
    // WiredTigerSessionRegister(session);
    return session;
  }

  void WiredTigerTableFind(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = Isolate::GetCurrent();
    WiredTigerTable& that = Unwrap<WiredTigerTable>(args.Holder());
    if (args.Length() >= 1) {
      if (!args[0]->IsArray() && !args[0]->IsUndefined()) {
        THROW(Exception::TypeError, "If provided the argument must be an array of conditions");
      }
    }

    std::vector<QueryCondition>* conditions;
    Local<Array> arr;
    if (args.Length() >= 1 && args[0]->IsArray()) {
      arr = Local<Array>::Cast(args[0]);
    }
    else {
      arr = Array::New(isolate, 0);
    }
    Local<Context> context = isolate->GetCurrentContext();


    FindOptions findOptions = {
      NULL,
      NULL,
      NULL,
      NULL
    };

    if (args.Length() == 2) {
      FindCursorExtractFindOptions(Local<Object>::Cast(args[1]), &findOptions);
    }
    WiredTigerSession* session;
    if (findOptions.session != NULL) {
      session = findOptions.session;
    }
    else {
      session = GetSession(that.getDb());
    }
    findOptions.session = session;

    conditions = new std::vector<QueryCondition>(arr->Length()); // deleted when the find cursor closes
    int result = parseConditions(isolate, context, &arr, conditions, that.getValueFormats());
    if (result != 0) {
      return ThrowExtractError(result, args);
    }
    Local<Object> object = wiredtiger::binding::FindCursorGetNew(
      that.getTableName(),
      conditions,
      &findOptions
    );
    if (findOptions.keyFormat) {
      free(findOptions.keyFormat);
    }
    if (findOptions.valueFormat) {
      free(findOptions.valueFormat);
    }
    if (findOptions.columns) {
      free(findOptions.columns);
    }
    Return(object, args);
  }

  // stupid - I extracted the wrong function, this is used from one place
  int ExtractDocuments(
    const FunctionCallbackInfo<Value>& args,
    WiredTigerTable* that,
    Local<Array> arr,
    std::vector<std::unique_ptr<EntryOfVectors>>* converted
  ) {
    Isolate* isolate = Isolate::GetCurrent();
    Local<Context> context = isolate->GetCurrentContext();
    int error;
    for (uint32_t i = 0; i < arr->Length(); i++) {
      Local<Value> holder = arr->Get(context, i).ToLocalChecked();
      if (!holder->IsArray()) {
        THROW_RETURN(Exception::TypeError, "argument must be an array where each entry is this form [[key], [value1, ...valuen]]", -1);
      }
      Local<Array> kvPair = Local<Array>::Cast(holder);

      Local<Value> keyHolder = kvPair->Get(context, 0).ToLocalChecked();
      Local<Value> valuesHolder = kvPair->Get(context, 1).ToLocalChecked();
      if (kvPair->Length() != 2 || !keyHolder->IsArray() || !valuesHolder->IsArray()) {
        THROW_RETURN(Exception::TypeError, "argument must be an array where each entry is this form [[key], [value1, ...valuen]]", -1);
      }

      Local<Array> keys = Local<Array>::Cast(keyHolder);
      Local<Array> values = Local<Array>::Cast(valuesHolder);
      size_t keySize = that->getKeyFormats()->size();
      size_t valueSize = that->getValueFormats()->size();
      std::vector<QueryValue>* convertedKeys = new std::vector<QueryValue>(keySize); // deleted with the EntryOfVectors
      std::vector<QueryValue>* convertedValues = new std::vector<QueryValue>(valueSize); // deleted with the EntryOfVectors
      error = extractValues(
        keys,
        isolate,
        context,
        convertedKeys,
        that->getKeyFormats()
      );
      if (error) {
        THROW_RETURN(Exception::TypeError, "Invalid key type", error);
      }
      if (!valuesHolder->IsArray()) {
        THROW_RETURN(Exception::TypeError, "Second argument must be an array where each entry is this form [key, [value1, ...valuen]]", i);
      }
      error = extractValues(
        values,
        isolate,
        context,
        convertedValues,
        that->getValueFormats()
      );
      if (error) {
        // freeConvertedKVPairs(&converted);
        THROW_RETURN(Exception::TypeError, "Invalid value type", error);
      }
      (*converted)[i] = std::unique_ptr<EntryOfVectors>(new EntryOfVectors());
      (*converted)[i]->keyArray = convertedKeys;
      (*converted)[i]->valueArray = convertedValues;
    }
    return 0;
  }

  void WiredTigerTableUpdateMany(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = Isolate::GetCurrent();
    Local<Context> context = isolate->GetCurrentContext();
    WiredTigerTable& that = Unwrap<WiredTigerTable>(args.Holder());
    if (args.Length() != 2) {
      if ((!args[0]->IsArray() && !args[0]->IsUndefined()) || (!args[1]->IsArray())) {
        THROW(Exception::TypeError, "If provided the argument must be an array of conditions");
      }
    }
    Local<Array> conditionsArray;
    if (args[0]->IsArray()) {
      conditionsArray = Local<Array>::Cast(args[0]);
    }
    else {
      conditionsArray = Array::New(isolate, 0);
    }
    Local<Array> newValuesArray;
    newValuesArray = Local<Array>::Cast(args[1]);

    std::vector<QueryCondition> conditions(conditionsArray->Length());
    int result = parseConditions(isolate, context, &conditionsArray, &conditions, that.getValueFormats());
    std::vector<Format>* valueFormats = that.getValueFormats();
    std::vector<QueryValue> newValues(valueFormats->size());
    if (newValues.size() != newValuesArray->Length()) {
      THROW(Exception::TypeError, "Values array length must match");
    }
    result = extractValues(newValuesArray, isolate, context, &newValues, valueFormats);
    if (result != 0) {
      return ThrowExtractError(result, args);
    }
    int updatedCount;
    WiredTigerSession* session = GetSession(that.getDb());
    result = session->beginTransaction(NULL);
    if (result) {
      THROW(Exception::TypeError, wiredtiger_strerror(result));
    }
    result = that.updateMany(
      session,
      &conditions,
      &newValues,
      &updatedCount
    );

    if (result) {
      session->rollbackTransaction(NULL);
      session->close(NULL);
      THROW(Exception::TypeError, wiredtiger_strerror(result));
    }
    result = session->commitTransaction(NULL);
    if (result) {
      THROW(Exception::TypeError, wiredtiger_strerror(result));
    }

    Return(Int32::New(isolate, updatedCount), args);
  }

  void WiredTigerTableDeleteMany(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = Isolate::GetCurrent();
    Local<Context> context = isolate->GetCurrentContext();
    WiredTigerTable& that = Unwrap<WiredTigerTable>(args.Holder());
    if (args.Length() != 1) {
      if (!args[0]->IsArray() && !args[0]->IsUndefined()) {
        THROW(Exception::TypeError, "If provided the argument must be an array of conditions");
      }
    }


    Local<Array> arr;
    if (args.Length() >= 1 && args[0]->IsArray()) {
      arr = Local<Array>::Cast(args[0]);
    }
    else {
      arr = Array::New(isolate, 0);
    }

    WiredTigerSession* session = GetSession(that.getDb());
    int result;

    std::vector<QueryCondition> conditions(arr->Length());
    result = parseConditions(isolate, context, &arr, &conditions, that.getValueFormats());
    if (result != 0) {
      return ThrowExtractError(result, args);
    }
    int deletedCount = 0;
    result = session->beginTransaction(NULL);
    if (result) {
      THROW(Exception::TypeError, wiredtiger_strerror(result));
    }
    result = that.deleteMany(session, &conditions, &deletedCount);
    if (result != 0) {
      session->rollbackTransaction(NULL);
      THROW(Exception::TypeError, wiredtiger_strerror(result));
    }
    result = session->commitTransaction(NULL);
    if (result) {
      THROW(Exception::TypeError, wiredtiger_strerror(result));
    }
    Return(Int32::New(isolate, deletedCount), args);
  }

  void WiredTigerTableInsertMany(const FunctionCallbackInfo<Value>& args) {
    WiredTigerTable& that = Unwrap<WiredTigerTable>(args.Holder());
    if (args.Length() != 1 || !args[0]->IsArray()) {
      THROW(Exception::TypeError, "Must specify an array of entries.");
    }

    Local<Array> arr = Local<Array>::Cast(args[0]);
    int error;

    std::vector<std::unique_ptr<EntryOfVectors>> converted(arr->Length());

    WiredTigerSession* session = GetSession(that.getDb());
    error = ExtractDocuments(args, &that, arr, &converted);
    if (error) {
      session->close(NULL);
      // no need to throw because ExtractMultiCursorConditions already did
      return;
    }
    bool hasTransaction = false;
    if (converted.size() > 1) {
      error = session->beginTransaction(NULL);
      hasTransaction = true;
      if (error) {
        THROW(Exception::TypeError, wiredtiger_strerror(error));
      }
    }
    error = that.insertMany(session, &converted);
    if (error) {
      if (hasTransaction) {
        session->rollbackTransaction(NULL);
      }
      THROW(Exception::TypeError, wiredtiger_strerror(error));
    }
    if (hasTransaction) {
      error = session->commitTransaction(NULL);
    }
    error = session->close(NULL);
    // freeConvertedKVPairs(&converted);
    if (error) {
      THROW(Exception::TypeError, wiredtiger_strerror(error));
    }
  }


  void WiredTigerTableCreateIndex(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = Isolate::GetCurrent();
    WiredTigerTable& that = Unwrap<WiredTigerTable>(args.Holder());
    if (args.Length() != 2 || !args[0]->IsString() || !args[1]->IsString()) {
      THROW(Exception::TypeError, "Must specify a name and config string.");
    }
    char* indexName = ArgToCharPointer(isolate, args[0]);
    char* config = ArgToCharPointer(isolate, args[1]);
    WiredTigerSession* session = GetSession(that.getDb());
    int error = that.createIndex(session, indexName, config);
    free(indexName);
    free(config);
    if (error) {
      THROW(Exception::TypeError, wiredtiger_strerror(error));
    }
  }

  void WiredTigerTableInit(Local<Object> target) {
    Isolate* isolate = Isolate::GetCurrent();
    Local<Context> context = isolate->GetCurrentContext();

    Local<FunctionTemplate> tmpl = NewFunctionTemplate(isolate, WiredTigerTableNew);
    WiredTigerTableConstructorTmpl.Reset(isolate, tmpl);
    tmpl->SetClassName(NewLatin1String(isolate, "WiredTigerTable"));
    tmpl->InstanceTemplate()->SetInternalFieldCount(1);

    // WiredTigerTable.prototype
    Local<ObjectTemplate> proto = tmpl->PrototypeTemplate();
    Local<Signature> sig = Signature::New(isolate, tmpl);
    tmpl->SetClassName(NewLatin1String(isolate, "WiredTigerTable"));
    tmpl->InstanceTemplate()->SetInternalFieldCount(1);


    // at some point should probably separate out the raw session implementation from these helpers
    proto->Set(NewLatin1String(isolate, "find"), NewFunctionTemplate(isolate, WiredTigerTableFind, Local<Value>(), sig));
    proto->Set(NewLatin1String(isolate, "insertMany"), NewFunctionTemplate(isolate, WiredTigerTableInsertMany, Local<Value>(), sig));
    proto->Set(NewLatin1String(isolate, "updateMany"), NewFunctionTemplate(isolate, WiredTigerTableUpdateMany, Local<Value>(), sig));
    proto->Set(NewLatin1String(isolate, "deleteMany"), NewFunctionTemplate(isolate, WiredTigerTableDeleteMany, Local<Value>(), sig));
    proto->Set(NewLatin1String(isolate, "createIndex"), NewFunctionTemplate(isolate, WiredTigerTableCreateIndex, Local<Value>(), sig));
    Local<Function> fn = tmpl->GetFunction(Isolate::GetCurrent()->GetCurrentContext()).ToLocalChecked();
    target->Set(context, String::NewFromOneByte(isolate, (const uint8_t*)"WiredTigerTable", NewStringType::kNormal).ToLocalChecked(), fn).FromJust();

  }
}
