#ifndef SQLITE3_CIPHER_DB_H_
#define SQLITE3_CIPHER_DB_H_

#include <node.h>
#include <node_object_wrap.h>
#include "sqlite3.h"
#include <uv.h>
#include <vector>

namespace sqlite3_cipher{

using namespace v8;

class Sqlite3CipherDb;

class Sqlite3CipherStmt : public node::ObjectWrap
{
public:
	static void Init();

	static Local<Object> prepare(Local<Object> db, Local<String> sql);

private:
	explicit Sqlite3CipherStmt(Sqlite3CipherDb* pDB, std::string sql);
	~Sqlite3CipherStmt();

	static void New(const FunctionCallbackInfo<Value>& args);

	// �󶨲���
	static void bind(const FunctionCallbackInfo<Value>& args);
	// ִ��
	static void exec(const FunctionCallbackInfo<Value>& args);

	static Persistent<Function> constructor;

	std::string           m_strSql;
	sqlite3_stmt*         m_pStmts;
	Sqlite3CipherDb*      m_pDB;
};

class Sqlite3CipherDb : public node::ObjectWrap
{
public:
    static void Init();
	static void connect(const FunctionCallbackInfo<Value>& args);

private:
    explicit Sqlite3CipherDb();
    ~Sqlite3CipherDb();

    static void New(const FunctionCallbackInfo<Value>& args);


    static void exec(const FunctionCallbackInfo<Value>& args);
    static void close(const FunctionCallbackInfo<Value>& args);
	static void prepare(const FunctionCallbackInfo<Value>& args);

public:
    static Persistent<Function> constructor;
    sqlite3* m_pDB; 
};

}
#endif