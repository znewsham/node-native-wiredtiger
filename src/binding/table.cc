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
    // TODO: defer this until the first time it's used? Hard if another session wants to use it.
    // Will need to expose it through the binding or better yet, expose relevant UX (e.g., create index) through the table
    // int result = table->initTable(NULL);
    //if (result == 0) {
      handle->SetAlignedPointerInInternalField(0, table);
      Return(args.This(), args);
    // }
    // else {
    //   THROW(Exception::TypeError, wiredtiger_strerror(result));
    // }
    free(tableName);
    free(config);
  }

  WiredTigerSession* GetSession(WiredTigerDB* db) {
    WT_SESSION* wtSession;
    db->conn->open_session(db->conn, NULL, NULL, &wtSession);
    WiredTigerSession* session = new WiredTigerSession(wtSession);
    WiredTigerSessionRegister(session);
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
    that.initTable(session);

    conditions = new std::vector<QueryCondition>(arr->Length()); // deleted when the find cursor closes
    int result = parseConditions(isolate, context, &arr, conditions, that.getValueFormats());
    if (result != 0) {
      if (result == CONDITION_NOT_OBJECT) {
        THROW(Exception::TypeError, "Invalid Condition (not an object)");
      }
      else if (result == INDEX_NOT_STRING) {
        THROW(Exception::TypeError, "Invalid Condition (index not a string)");
      }
      else if (result == OPERATION_NOT_INTEGER) {
        THROW(Exception::TypeError, "Invalid Condition (operation not an integer)");
      }
      else if (result == VALUES_NOT_ARRAY) {
        THROW(Exception::TypeError, "Invalid Condition (values not an array)");
      }
      else if (result == CONDITIONS_NOT_ARRAY) {
        THROW(Exception::TypeError, "Invalid Condition (conditions not an array)");
      }
      else if (result == INVALID_VALUE_TYPE) {
        THROW(Exception::TypeError, "Invalid Condition (invalid value type)");
      }
      else if (result == VALUES_AND_CONDITIONS) {
        THROW(Exception::TypeError, "Invalid Condition (can't specify sub conditions and values)");
      }
      else if (result == INVALID_OPERATOR) {
        THROW(Exception::TypeError, "Invalid Condition (AND/OR mismatch with values/conditions)");
      }
      else if (result == NO_VALUES_OR_CONDITIONS) {
        THROW(Exception::TypeError, "Invalid Condition (missing values or conditions)");
      }
      else if (result == EMPTY_CONDITIONS) {
        THROW(Exception::TypeError, "Invalid Condition (empty conditions)");
      }
      else if (result == EMPTY_VALUES) {
        THROW(Exception::TypeError, "Invalid Condition (empty values)");
      }
      else {
        THROW(Exception::TypeError, "Unknown error");
      }
      return;
    }

    // free(tableName); can't free it here
    if (result != 0) {
      THROW(Exception::TypeError, wiredtiger_strerror(result));
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

  void WiredTigerTableInsertMany(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = Isolate::GetCurrent();
    WiredTigerTable& that = Unwrap<WiredTigerTable>(args.Holder());
    Local<Context> context = isolate->GetCurrentContext();
    if (args.Length() != 1 || !args[0]->IsArray()) {
      THROW(Exception::TypeError, "Must specify an array of entries.");
    }

    Local<Array> arr = Local<Array>::Cast(args[0]);
    int error;

    std::vector<std::unique_ptr<EntryOfVectors>> converted(arr->Length());

    WiredTigerSession* session = GetSession(that.getDb());
    for (uint32_t i = 0; i < arr->Length(); i++) {
      Local<Value> holder = arr->Get(context, i).ToLocalChecked();
      if (!holder->IsArray()) {
        THROW(Exception::TypeError, "argument must be an array where each entry is this form [[key], [value1, ...valuen]]");
      }
      Local<Array> kvPair = Local<Array>::Cast(holder);

      Local<Value> keyHolder = kvPair->Get(context, 0).ToLocalChecked();
      Local<Value> valuesHolder = kvPair->Get(context, 1).ToLocalChecked();
      if (kvPair->Length() != 2 || !keyHolder->IsArray() || !valuesHolder->IsArray()) {
        THROW(Exception::TypeError, "argument must be an array where each entry is this form [[key], [value1, ...valuen]]");
      }

      Local<Array> keys = Local<Array>::Cast(keyHolder);
      Local<Array> values = Local<Array>::Cast(valuesHolder);
      size_t keySize = that.getKeyFormats()->size();
      size_t valueSize = that.getValueFormats()->size();
      std::vector<QueryValueOrWT_ITEM>* convertedKeys = new std::vector<QueryValueOrWT_ITEM>(keySize); // deleted with the EntryOfVectors
      std::vector<QueryValueOrWT_ITEM>* convertedValues = new std::vector<QueryValueOrWT_ITEM>(valueSize); // deleted with the EntryOfVectors
      error = that.initTable(session);
      if (error) {
        session->session->close(session->session, NULL);
        THROW(Exception::TypeError, wiredtiger_strerror(error));
      }
      error = extractValues(
        keys,
        isolate,
        context,
        convertedKeys,
        that.getKeyFormats()
      );
      if (error) {
        session->session->close(session->session, NULL);
        THROW(Exception::TypeError, "Invalid key type");
      }
      if (!valuesHolder->IsArray()) {
        THROW(Exception::TypeError, "Second argument must be an array where each entry is this form [key, [value1, ...valuen]]");
      }
      error = extractValues(
        values,
        isolate,
        context,
        convertedValues,
        that.getValueFormats()
      );
      if (error) {
        // freeConvertedKVPairs(&converted);
        session->session->close(session->session, NULL);
        THROW(Exception::TypeError, "Invalid value type");
      }
      converted[i] = std::unique_ptr<EntryOfVectors>(new EntryOfVectors());
      converted[i]->keyArray = convertedKeys;
      converted[i]->valueArray = convertedValues;
    }
    error = that.insertMany(session, &converted);
    error = session->session->close(session->session, NULL);
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
    proto->Set(NewLatin1String(isolate, "createIndex"), NewFunctionTemplate(isolate, WiredTigerTableCreateIndex, Local<Value>(), sig));
    Local<Function> fn = tmpl->GetFunction(Isolate::GetCurrent()->GetCurrentContext()).ToLocalChecked();
    target->Set(context, String::NewFromOneByte(isolate, (const uint8_t*)"WiredTigerTable", NewStringType::kNormal).ToLocalChecked(), fn).FromJust();

  }
}
