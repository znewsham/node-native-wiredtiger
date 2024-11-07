#include <map>
#include <set>
#include <node.h>
#include <string_view>
#include <string>
#include <nan.h>
#include <v8-local-handle.h>
#include <v8-object.h>
#include <v8-internal.h>
#include "v8-version.h"
#include "../helpers.h"
#include "./helpers.h"



// this is really just for performance testing - how much better/worse is a C map than a JS map/wiredtiger DB
namespace wiredtiger::binding {

  typedef struct BufferEntry {
    int size;
    char* buffer;
    v8::Global<v8::Value>* reference;
  } BufferEntry;

  typedef struct Entry {
    string value;
    int hash;
    bool operator==(const Entry &other) const {
      return hash == other.hash && value == other.value;
    }

  } Entry;

  struct EntryHash
  {
      std::size_t operator()(const Entry& s) const noexcept
      {
        return s.hash;
      }
  };

  struct EntryComparator
  {
    bool operator()(const Entry& a, const Entry& b) const {
      return a.hash - b.hash;
    }
  };

  class Map {
    private:
    public:
      unordered_map<string_view, BufferEntry> m;
      unordered_map<int, string> m_hash;
      unordered_map<int, void*> m_nested;
      unordered_map<Entry, string, EntryHash> m_entry;
      Map() {

      }
  };

  Persistent<FunctionTemplate> MapConstructorTmpl;

  bool useHash = false;
  bool useEntry = false;
  bool useNested = false;

  void MapSet(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = Isolate::GetCurrent();
    v8::EscapableHandleScope handle_scope(isolate);
    Map& that = Unwrap<Map>(args.Holder());
    if (args.Length() != 2 || !args[0]->IsString() || !args[1]->IsObject()) {
      THROW(Exception::TypeError, "Wants two strings");
    }
    // Local<Object> obj = args[1]->ToObject(isolate->GetCurrentContext()).ToLocalChecked();

    int size = node::Buffer::Length(args[1]);
    char* value = node::Buffer::Data(args[1]);

    if (useNested) {
      Entry e {
        string(ArgToCharPointer(isolate, args[0])),
        Local<String>::Cast(args[0])->GetIdentityHash()
      };
      if (that.m_nested.find(e.hash) != that.m_nested.end()) {
        map<string, string>* nestedMap = (map<string, string>*)that.m_nested.at(e.hash);
        nestedMap->insert(pair<string, string>{ e.value, string(value) });
      }
      else {
        map<string, string>* nestedMap = new map<string, string>();
        nestedMap->insert(pair<string, string>{e.value, string(value)});
        that.m_nested.insert(pair<int, map<string, string>*>{e.hash, nestedMap});
      }
    }
    else if (useEntry) {
      Entry e {
        string(ArgToCharPointer(isolate, args[0])),
        Local<String>::Cast(args[0])->GetIdentityHash()
      };
      that.m_entry.insert(pair<Entry, string>(e, string(value)));
    }
    else if (useHash) {
      that.m_hash.insert(pair<int, string>(Local<String>::Cast(args[0])->GetIdentityHash(), string(value)));
    }
    else {
      char* key = ArgToCharPointer(isolate, args[0]);
      v8::Global<v8::Value>* reference = new v8::Global<v8::Value>();
      // Persistent<Value>::New(Handle<Value>::Cast(args[1]));
      reference->Reset(isolate, args[1]);
      that.m.insert(pair<string_view, BufferEntry>(
        string_view(key),
        BufferEntry { size, value, reference }
      ));
    }
  }

  char _key[1000];
  void MapGet(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = Isolate::GetCurrent();
    Map& that = Unwrap<Map>(args.Holder());
    (void)that;
    (void)isolate;
    if (args.Length() != 1 || !args[0]->IsString()) {
      THROW(Exception::TypeError, "Wants one string");
    }
    // StringToCharPointer(isolate, Local<String>::Cast(args[0]), _key);
    Local<String> aString = Local<String>::Cast(args[0]);
    BufferEntry value;

    // if (useNested) {
    //   Entry e {
    //     std::string(ArgToCharPointer(isolate, args[0])),
    //     aString->GetIdentityHash()
    //   };
    //   map<string, string>* nested = (map<string, string>*)that.m_nested.at(e.hash);
    //   value = nested->at(string(e.value));
    // }
    // else if (useEntry) {
    //   Entry e {
    //     std::string(ArgToCharPointer(isolate, args[0])),
    //     aString->GetIdentityHash()
    //   };
    //   value = that.m_entry.at(e);
    // }
    // else if (useHash) {
    //   value = that.m_hash.at(aString->GetIdentityHash());
    // }
    // else {
      aString->WriteUtf8(isolate, _key);
      value = that.m.at(string_view(_key));
    // }
    // String::NewFromUtf8(isolate, value.data()).ToLocalChecked();
    // char* valueCharPointer = (char*)value.data();
    Local<Value> buff = value.reference->Get(isolate);
    Return(
      buff,
      args
    );
    // printf("Got: %d\n", node::Buffer::Length();
    // free(key);
  }

  void MapDelete(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = Isolate::GetCurrent();
    Map& that = Unwrap<Map>(args.Holder());
    if (args.Length() != 1 || !args[0]->IsString()) {
      THROW(Exception::TypeError, "Wants one string");
    }
    char* key = ArgToCharPointer(isolate, args[0]);
    that.m.erase(string_view(key));
    free(key);
  }

  void MapNew(const FunctionCallbackInfo<Value>& args) {
    Local<Object> handle = args.This();
    Map* myMap = new Map();
    handle->SetAlignedPointerInInternalField(0, myMap);
    Return(args.This(), args);
  }

  void MapInit(Local<Object> target) {
    Isolate* isolate = Isolate::GetCurrent();
    Local<Context> context = isolate->GetCurrentContext();

    Local<FunctionTemplate> tmpl = NewFunctionTemplate(isolate, MapNew);
    MapConstructorTmpl.Reset(isolate, tmpl);
    tmpl->SetClassName(NewLatin1String(isolate, "Map"));
    // // Guard which only allows these methods to be called on a fiber; prevents
    Local<Signature> sig = Signature::New(isolate, tmpl);
    tmpl->InstanceTemplate()->SetInternalFieldCount(1);

    // WiredTigerDB.prototype
    Local<ObjectTemplate> proto = tmpl->PrototypeTemplate();
    proto->Set(NewLatin1String(isolate, "set"), NewFunctionTemplate(isolate, MapSet, Local<Value>(), sig));
    proto->Set(NewLatin1String(isolate, "get"), NewFunctionTemplate(isolate, MapGet, Local<Value>(), sig));
    proto->Set(NewLatin1String(isolate, "delete"), NewFunctionTemplate(isolate, MapDelete, Local<Value>(), sig));
    Local<Function> fn = tmpl->GetFunction(Isolate::GetCurrent()->GetCurrentContext()).ToLocalChecked();
    // uni::SetAccessor(isolate, fn, uni::NewLatin1String(isolate, "fibersCreated"), GetFibersCreated);

    // // Global Fiber
    target->Set(context, String::NewFromOneByte(isolate, (const uint8_t*)"Map", NewStringType::kNormal).ToLocalChecked(), fn).FromJust();
    // uni::Reset(isolate, fiber_object, fn);
  }
}
