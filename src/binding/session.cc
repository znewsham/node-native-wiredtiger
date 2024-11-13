#include <map>
#include "v8-version.h"
#include "../wiredtiger/session.h"
#include "../wiredtiger/cursor.h"
#include "../helpers.h"
#include "cursor.h"
#include "helpers.h"
#include "../wiredtiger/types.h"


namespace wiredtiger::binding {
  Persistent<FunctionTemplate> WiredTigerSessionConstructorTmpl;
  void WiredTigerSessionCreate(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = Isolate::GetCurrent();
    WiredTigerSession& that = Unwrap<WiredTigerSession>(args.Holder());
    if (args.Length() != 2 || !args[0]->IsString() || !args[1]->IsString()) {
      THROW(Exception::TypeError, "Must take two string arguments");
    }
    char* typeAndName = ArgToCharPointer(isolate, args[0]);
    char* specs = ArgToCharPointer(isolate, args[1]);
    int result = that.create(typeAndName, specs);
    if (result) {
      THROW(Exception::TypeError, that.strerror(result));
    }
    free(typeAndName);
    free(specs);
    Return(Integer::New(isolate, result), args);
  }

  void WiredTigerSessionOpenCursor(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = Isolate::GetCurrent();
    WiredTigerSession& that = Unwrap<WiredTigerSession>(args.Holder());
    if (
      args.Length() != 2
      || !args[0]->IsString()
      || (!args[1]->IsString() && !args[1]->IsNull())
    ) {
      THROW(Exception::TypeError, "Takes two args. The first is a string, the second is a string or null");
    }

    char* uri = ArgToCharPointer(isolate, args[0]);
    char* config = NULL;
    if(args[1]->IsString()) {
      config = ArgToCharPointer(isolate, args[1]);
    }

    Local<Object> object = wiredtiger::binding::CursorGetNew(&that, uri, config);
    free(uri);
    if (config != NULL) {
      free(config);
    }
    Return(object, args);
  }

  void WiredTigerSessionClose(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = Isolate::GetCurrent();
    WiredTigerSession& that = Unwrap<WiredTigerSession>(args.Holder());
    int result = that.close(NULL);
    if (result != 0) {
      THROW(Exception::TypeError, that.strerror(result));
    }

    Return(Integer::New(isolate, result), args);
  }

  static map<WiredTigerSession*, v8::Global<v8::Object>*> existingSessions;

  static Local<Object> WiredTigerSessionNewInstance() {
    Isolate* isolate = Isolate::GetCurrent();
    Local<Function> fn = Deref(isolate, WiredTigerSessionConstructorTmpl)->GetFunction(isolate->GetCurrentContext()).ToLocalChecked();
    Local<Object> obj = fn->NewInstance(isolate->GetCurrentContext()).ToLocalChecked();
    return obj;
  }

  void WiredTigerSessionCleanup(const v8::WeakCallbackInfo<WiredTigerSession>& data){
    WiredTigerSession* session = (WiredTigerSession*)data.GetParameter();
    v8::Global<v8::Object>* persistent = existingSessions.at(session);
    existingSessions.erase(session);
    persistent->Reset(Isolate::GetCurrent(), v8::Local<v8::Object>{});
    printf("Resetting: %p | ", session);
    printf("%p | ", session->getWTSession());
    printf("%p\n", session->getWTSession()->app_private);
    delete session;
    delete persistent;
  }

  static void WiredTigerSessionRegister(WiredTigerSession* session, Local<Object> obj) {
    Isolate* isolate = Isolate::GetCurrent();
    obj->SetAlignedPointerInInternalField(0, session);
    v8::Global<v8::Object>* persistent = new v8::Global<v8::Object>();
    persistent->Reset(isolate, obj);
    printf("Registering: %p\n", session);
    persistent->SetWeak(session, WiredTigerSessionCleanup, v8::WeakCallbackType::kParameter);
    existingSessions.insert(pair<WiredTigerSession*, v8::Global<v8::Object>*>{ session, persistent });
  }

  void WiredTigerSessionRegister(WiredTigerSession* session) {
    Local<Object> obj = WiredTigerSessionNewInstance();
    WiredTigerSessionRegister(session, obj);
  }
  Local<Object> WiredTigerSessionGetOrCreate(WT_SESSION *session) {
    Isolate* isolate = Isolate::GetCurrent();
    WiredTigerSession* wtSession = WiredTigerSession::WiredTigerSessionForSession(session);
    map<WiredTigerSession*, v8::Global<v8::Object>*>::iterator index = existingSessions.find(wtSession);
    if (index != existingSessions.end()) {
      v8::Global<v8::Object>* existing = index->second;
      return existing->Get(isolate);
    }
    Local<Object> obj = WiredTigerSessionNewInstance();
    WiredTigerSessionRegister(wtSession, obj);
    return obj;
  }

  void WiredTigerSessionJoin(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = Isolate::GetCurrent();
    WiredTigerSession& that = Unwrap<WiredTigerSession>(args.Holder());
    if (
      args.Length() != 3
      || !args[0]->IsObject()
      || !args[1]->IsObject()
      || !args[2]->IsString()
    ) {
      THROW(Exception::TypeError, "Takes two cursors and a string");
    }

    Cursor& joinCursor = Unwrap<Cursor>(Local<Object>::Cast(args[0]));
    Cursor& refCursor = Unwrap<Cursor>(Local<Object>::Cast(args[1]));
    char* config = ArgToCharPointer(isolate, args[2]);

    int result = that.join(&joinCursor, &refCursor, config);
    free(config);
    if (result != 0) {
      THROW(Exception::TypeError, wiredtiger_strerror(result));
      return;
    }

    Return(Integer::New(isolate, result), args);
  }

  void WiredTigerSessionBeginTransaction(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = Isolate::GetCurrent();
    WiredTigerSession& that = Unwrap<WiredTigerSession>(args.Holder());
    if (
      args.Length() != 1
      || !args[0]->IsString()
    ) {
      THROW(Exception::TypeError, "Takes a string");
    }

    char* config = ArgToCharPointer(isolate, args[0]);

    int result = that.beginTransaction(config);
    free(config);
    if (result != 0) {
      THROW(Exception::TypeError, wiredtiger_strerror(result));
      return;
    }

    Return(Integer::New(isolate, result), args);
  }

  void WiredTigerSessionPrepareTransaction(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = Isolate::GetCurrent();
    WiredTigerSession& that = Unwrap<WiredTigerSession>(args.Holder());
    if (
      args.Length() != 1
      || !args[0]->IsString()
    ) {
      THROW(Exception::TypeError, "Takes a string");
    }

    char* config = ArgToCharPointer(isolate, args[0]);

    int result = that.prepareTransaction(config);
    free(config);
    if (result != 0) {
      THROW(Exception::TypeError, wiredtiger_strerror(result));
      return;
    }

    Return(Integer::New(isolate, result), args);
  }

  void WiredTigerSessionRollbackTransaction(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = Isolate::GetCurrent();
    WiredTigerSession& that = Unwrap<WiredTigerSession>(args.Holder());
    if (
      args.Length() != 1
      || !args[0]->IsString()
    ) {
      THROW(Exception::TypeError, "Takes a string");
    }

    char* config = ArgToCharPointer(isolate, args[0]);

    int result = that.rollbackTransaction(config);
    free(config);
    if (result != 0) {
      THROW(Exception::TypeError, wiredtiger_strerror(result));
      return;
    }

    Return(Integer::New(isolate, result), args);
  }

  void WiredTigerSessionCommitTransaction(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = Isolate::GetCurrent();
    WiredTigerSession& that = Unwrap<WiredTigerSession>(args.Holder());
    if (
      args.Length() != 1
      || !args[0]->IsString()
    ) {
      THROW(Exception::TypeError, "Takes a string");
    }

    char* config = ArgToCharPointer(isolate, args[0]);

    int result = that.commitTransaction(config);
    free(config);
    if (result != 0) {
      THROW(Exception::TypeError, wiredtiger_strerror(result));
      return;
    }

    Return(Integer::New(isolate, result), args);
  }

  void WiredTigerSessionNew(const FunctionCallbackInfo<Value>& args) {
  }

  void WiredTigerSessionInit(Local<Object> target) {
    Isolate* isolate = Isolate::GetCurrent();

    Local<FunctionTemplate> tmpl = NewFunctionTemplate(isolate, WiredTigerSessionNew);
    WiredTigerSessionConstructorTmpl.Reset(isolate, tmpl);
    tmpl->SetClassName(NewLatin1String(isolate, "WiredTigerSession"));
    tmpl->InstanceTemplate()->SetInternalFieldCount(1);

    // WiredTigerSession.prototype
    Local<ObjectTemplate> proto = tmpl->PrototypeTemplate();
    Local<Signature> sig = Signature::New(isolate, tmpl);
    proto->Set(NewLatin1String(isolate, "create"), NewFunctionTemplate(isolate, WiredTigerSessionCreate, Local<Value>(), sig));
    proto->Set(NewLatin1String(isolate, "close"), NewFunctionTemplate(isolate, WiredTigerSessionClose, Local<Value>(), sig));

    proto->Set(NewLatin1String(isolate, "openCursor"), NewFunctionTemplate(isolate, WiredTigerSessionOpenCursor, Local<Value>(), sig));
    proto->Set(NewLatin1String(isolate, "join"), NewFunctionTemplate(isolate, WiredTigerSessionJoin, Local<Value>(), sig));

    proto->Set(NewLatin1String(isolate, "beginTransaction"), NewFunctionTemplate(isolate, WiredTigerSessionBeginTransaction, Local<Value>(), sig));
    proto->Set(NewLatin1String(isolate, "commitTransaction"), NewFunctionTemplate(isolate, WiredTigerSessionCommitTransaction, Local<Value>(), sig));
    proto->Set(NewLatin1String(isolate, "prepareTransaction"), NewFunctionTemplate(isolate, WiredTigerSessionPrepareTransaction, Local<Value>(), sig));
    proto->Set(NewLatin1String(isolate, "rollbackTransaction"), NewFunctionTemplate(isolate, WiredTigerSessionRollbackTransaction, Local<Value>(), sig));
  }
}
