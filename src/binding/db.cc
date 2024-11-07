#include "v8-version.h"
#include "../wiredtiger/db.h"
#include "../wiredtiger/custom_collator.h"
#include "../wiredtiger/custom_extractor.h"
#include "../wiredtiger/session.h"
#include "../helpers.h"
#include "session.h"
#include "helpers.h"
#include "cursor.h"
#include "custom_extractor.h"
#include "custom_collator.h"
#include <nan.h>
#include "../wiredtiger/types.h"

namespace wiredtiger::binding {
  Persistent<FunctionTemplate> WiredTigerDBConstructorTmpl;

  void WiredTigerDBOpen(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = Isolate::GetCurrent();
    WiredTigerDB& that = Unwrap<WiredTigerDB>(args.Holder());
    if (that.conn != NULL) {
      THROW(Exception::TypeError, "Already opened");
    }
    char* directory_path = NULL;
    char* options = NULL;
    int result;
    if (args.Length() == 0) {
      result = that.open(NULL, NULL);
    }
    else {
      if (args.Length() >= 1) {
        if (args[0]->IsString()) {
          directory_path = ArgToCharPointer(isolate, args[0]);
        }
        else if (!args[0]->IsNull()) {
          THROW(Exception::TypeError, "First argument should be null or a valid directory");
        }
      }
      if (args.Length() == 2) {
        if (args[1]->IsString()) {
          options = ArgToCharPointer(isolate, args[1]);
        }
        else if (!args[1]->IsNull()) {
          THROW(Exception::TypeError, "First argument should be null or a valid directory");
        }
      }
      else {
        THROW(Exception::TypeError, "open takes 0, 1 or 2 arguments");
      }
      result = that.open(directory_path, options);
      if (options != NULL) {
        free(options);
      }
      if (directory_path != NULL) {
        free(directory_path);
      }
      if (result) {
        THROW(Exception::TypeError, wiredtiger_strerror(result));
      }
    }
    Return(Integer::New(isolate, result), args);
  }
  void WiredTigerDBClose(const FunctionCallbackInfo<Value>& args) {
    WiredTigerDB& that = Unwrap<WiredTigerDB>(args.Holder());
    int result = that.conn->close(that.conn, NULL);
    that.conn = NULL;
    if (result != 0) {
      THROW(Exception::TypeError, "Couldn't close");
    }
  }

  void WiredTigerDBOpenSession(const FunctionCallbackInfo<Value>& args) {
    WiredTigerDB& that = Unwrap<WiredTigerDB>(args.Holder());
    if (that.conn == NULL) {
      THROW(Exception::TypeError, "Not opened");
    }
    char* config = NULL;
    if (args.Length() == 1) {
      if (!args[0]->IsString()) {
        THROW(Exception::TypeError, "Config must be a string");
      }
      config = ArgToCharPointer(Isolate::GetCurrent(), args[0]);
    }
    WT_SESSION *wtSession;
    that.conn->open_session(that.conn, NULL, config, &wtSession);

    Local<Object> object = wiredtiger::binding::WiredTigerSessionGetOrCreate(wtSession);

    Return(object, args);
  }

  void WiredTigerDBConfigureMethod(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = Isolate::GetCurrent();
    WiredTigerDB& that = Unwrap<WiredTigerDB>(args.Holder());
    if (
      args.Length() != 3
      || !args[0]->IsString()
      || !args[1]->IsString()
      || !args[2]->IsString()
      || !args[3]->IsString()
      || !args[4]->IsString()
    ) {
      THROW(Exception::TypeError, "Takes 5 strings");
    }
    if (that.conn == NULL) {
      THROW(Exception::TypeError, "Not opened");
    }

    char* method = ArgToCharPointer(isolate, args[0]);
    char* uriPrefix = ArgToCharPointer(isolate, args[1]);
    char* config = ArgToCharPointer(isolate, args[2]);
    char* type = ArgToCharPointer(isolate, args[3]);
    char* check = ArgToCharPointer(isolate, args[4]);

    int result = that.conn->configure_method(that.conn, method, uriPrefix, config, type, check);
    free(method);
    free(uriPrefix);
    free(config);
    free(type);
    free(check);
    if (result != 0) {
      THROW(Exception::TypeError, wiredtiger_strerror(result));
      return;
    }

    Return(Integer::New(isolate, result), args);
  }

  void WiredTigerDBAddExtractor(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = Isolate::GetCurrent();
    WiredTigerDB& that = Unwrap<WiredTigerDB>(args.Holder());
    if (
      args.Length() < 2
      || !args[0]->IsString()
      || !args[1]->IsObject()
      || (args.Length() > 2 && !args[2]->IsString() && !args[2]->IsUndefined())
    ) {
      THROW(Exception::TypeError, "Takes string, CustomExtractor, string");
    }
    if (that.conn == NULL) {
      THROW(Exception::TypeError, "Not opened");
    }

    char* name = ArgToCharPointer(isolate, args[0]);
    CustomExtractor& extractor = Unwrap<CustomExtractor>(args[1]->ToObject(isolate->GetCurrentContext()).ToLocalChecked());
    char* config = args.Length() > 2 && args[2]->IsString() ? ArgToCharPointer(isolate, args[2]) : NULL;
    that.addExtractor(name, &extractor);
  }

  void WiredTigerDBAddCollator(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = Isolate::GetCurrent();
    WiredTigerDB& that = Unwrap<WiredTigerDB>(args.Holder());
    if (
      args.Length() < 2
      || !args[0]->IsString()
      || !args[1]->IsObject()
      || (args.Length() > 4 && !args[4]->IsString() && !args[4]->IsUndefined())
    ) {
      THROW(Exception::TypeError, "Takes string, CustomCollator, string");
    }
    if (that.conn == NULL) {
      THROW(Exception::TypeError, "Not opened");
    }

    char* name = ArgToCharPointer(isolate, args[0]);
    CustomCollator& collator = Unwrap<CustomCollator>(args[1]->ToObject(isolate->GetCurrentContext()).ToLocalChecked());
    char* config = args.Length() > 4 && args[4]->IsString() ? ArgToCharPointer(isolate, args[1]) : NULL;
    that.addCollator(name, &collator);
  }

  void WiredTigerDBNew(const FunctionCallbackInfo<Value>& args) {
    Local<Object> handle = args.This();
    WiredTigerDB* db = new WiredTigerDB();
    handle->SetAlignedPointerInInternalField(0, db);
    Return(args.This(), args);
  }

  void WiredTigerDBInit(Local<Object> target) {
    Isolate* isolate = Isolate::GetCurrent();
    Local<Context> context = isolate->GetCurrentContext();

    Local<FunctionTemplate> tmpl = NewFunctionTemplate(isolate, WiredTigerDBNew);
    WiredTigerDBConstructorTmpl.Reset(isolate, tmpl);
    tmpl->SetClassName(NewLatin1String(isolate, "WiredTigerDB"));
    // // Guard which only allows these methods to be called on a fiber; prevents
    // // `fiber.run.call({})` from seg faulting.
    Local<Signature> sig = Signature::New(isolate, tmpl);
    tmpl->InstanceTemplate()->SetInternalFieldCount(1);

    // WiredTigerDB.prototype
    Local<ObjectTemplate> proto = tmpl->PrototypeTemplate();
    proto->Set(NewLatin1String(isolate, "open"), NewFunctionTemplate(isolate, WiredTigerDBOpen, Local<Value>(), sig));
    proto->Set(NewLatin1String(isolate, "close"), NewFunctionTemplate(isolate, WiredTigerDBClose, Local<Value>(), sig));
    proto->Set(NewLatin1String(isolate, "openSession"), NewFunctionTemplate(isolate, WiredTigerDBOpenSession, Local<Value>(), sig));
    proto->Set(NewLatin1String(isolate, "configureMethod"), NewFunctionTemplate(isolate, WiredTigerDBConfigureMethod, Local<Value>(), sig));
    proto->Set(NewLatin1String(isolate, "addExtractor"), NewFunctionTemplate(isolate, WiredTigerDBAddExtractor, Local<Value>(), sig));
    proto->Set(NewLatin1String(isolate, "addCollator"), NewFunctionTemplate(isolate, WiredTigerDBAddCollator, Local<Value>(), sig));
    Local<Function> fn = tmpl->GetFunction(Isolate::GetCurrent()->GetCurrentContext()).ToLocalChecked();
    // uni::SetAccessor(isolate, fn, uni::NewLatin1String(isolate, "fibersCreated"), GetFibersCreated);

    target->Set(context, String::NewFromOneByte(isolate, (const uint8_t*)"WiredTigerDB", NewStringType::kNormal).ToLocalChecked(), fn).FromJust();
    // uni::Reset(isolate, fiber_object, fn);
  }
}
