#include "sqlite3cipherdb.h"

namespace sqlite3_cipher
{
using v8;

Persistent<Function> Sqlite3CipherDb constructor;

Sqlite3CipherDb::Sqlite3CipherDb()
{
    m_pDB = NULL;
}
Sqlite3CipherDb::~Sqlite3CipherDb()
{

}

void Sqlite3CipherDb::Init(Isolate* isolate)
{
    // 准备构造函数
    Local<FunctionTemplate> tpl = FunctionTemplate::New(isolate, New);
    tpl->SetClassName(String::NewFromUtf8(isolate, "SqliteCipherDb"));
    tpl->InstanceTemplate()->SetInternalFieldCount(1);

    NODE_SET_PROTOTYPE_METHOD(tpl, "exec", exec);
    NODE_SET_PROTOTYPE_METHOD(tpl, "close", close);
    
    constructor.Reset(isolate, tpl->GetFunction());
}

void Sqlite3CipherDb::New(const FunctionCallbackInfo<Value>& args)
{
    Isolate* isolate = args.GetIsolate();

    if (args.IsContructCall()){
        Sqlite3CipherDb* obj = new Sqlite3CipherDb();
        obj->Wrap(args.This());
        args.GetReturnValue().Set(args.This());
    } else {
        // Normal Method 
        NewInstance(args);
    }
}

void Sqlite3CipherDb::NewInstance(const FunctionCallbackInfo<Value>& args)
{
    const int argc = 3;
    Local<Value> argv[argc] = {args[0], args[1], args[2]};
    Local<Function> cons = Local<Function>::New(isolate, constructor);
    Local<Context> context = isolate->GetCurrentContext();
    Local<Object> instance =
        cons->NewInstance(context, argc, argv).ToLocalChecked();
    args.GetReturnValue().Set(instance);
}
}