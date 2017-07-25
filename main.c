#include <node.h>
#include "sqplite3cipherdb.h"

namespace sqlite3_cipher
{
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::Isolate;
using v8::Local;
using v8::Null;
using v8::Object;
using v8::String;
using v8::Value;

void connect(const FunctionCallbackInfo<Value>& args)
{
    Sqlite3CipherDb::NewInstance(args);
}

void Init(Local<Object> exports, Local<Object> module){
    Sqlite3CipherDb::Init(exports->GetIsolate());
    NODE_SET_METHOD(exports, "connect", connect);
}
NODE_MODULE(sqlite3-cipher,Init);
}