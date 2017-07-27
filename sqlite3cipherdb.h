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

	static void prepare(const FunctionCallbackInfo<Value>& args);

private:
	explicit Sqlite3CipherStmt(Local<Sqlite3CipherDb> db);
	~Sqlite3CipherStmt();

	static void New(const FunctionCallbackInfo<Value>& args);

	// 绑定参数
	static void bind(const FunctionCallbackInfo<Value>& args);
	// 执行
	static void exec(const FunctionCallbackInfo<Value>& args);

	static Persistent<Function> constructor;

	sqlite3_stmt*         m_pStmts;
	Persistent<Sqlite3CipherDb>  m_db;
};

class Sqlite3CipherDb : public node::ObjectWrap
{
public:
    static void Init();
    static void NewInstance(const FunctionCallbackInfo<Value>& args);
	static void connect(const FunctionCallbackInfo<Value>& args);

private:
    explicit Sqlite3CipherDb();
    ~Sqlite3CipherDb();

    static void New(const FunctionCallbackInfo<Value>& args);

    static void exec(const FunctionCallbackInfo<Value>& args);
    static void close(const FunctionCallbackInfo<Value>& args);
	static void prepare(const FunctionCallbackInfo<Value>& args);

	static void exec_ing_(uv_work_t* req);
	static void exec_done_(uv_work_t* req, int status);

    static Persistent<Function> constructor;

    sqlite3* m_pDB; 

	friend class Sqlite3CipherStmt;
};

}
#endif