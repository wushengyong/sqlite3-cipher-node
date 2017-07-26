#include <node.h>
#include "sqlite3cipherdb.h"

namespace sqlite3_cipher
{
using namespace v8;

void connect(const FunctionCallbackInfo<Value>& args)
{
    Sqlite3CipherDb::NewInstance(args);
}

void Init(Local<Object> exports, Local<Object> module){
    Sqlite3CipherDb::Init();
    NODE_SET_METHOD(exports, "connect", connect);
}
NODE_MODULE(sqlite3cipher,Init);
}