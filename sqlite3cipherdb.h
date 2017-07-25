#ifndef SQLITE3_CIPHER_DB_H_
#define SQLITE3_CIPHER_DB_H_

#include <node.h>
#include <node_object_wrap.h>
#include "sqlite3.h"

namespace sqlite3_cipher{

class Sqlite3CipherDb : public node::ObjectWrap
{
public:
    static void Init(v8::Isolate* isolate);
    static void NewInstance(const v8::FunctionCallbackInfo<v8::Value>& args);

private:
    explicit Sqlite3CipherDb();
    ~Sqlite3CipherDb();

    static void New(const v8::FunctionCallbackInfo<v8::Value>& args);
    static void exec(const FunctionCallbackInfo<v8::Value>& args);
    static void close(const FunctionCallbackInfo<v8::Value>& args);
    static v8::Persistent<v8::Function> constructor;
    
    sqlite3_stmt* m_pDB; 
}

}
#endif