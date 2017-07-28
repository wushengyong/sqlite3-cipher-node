#include "sqlite3cipherdb.h"
#include <vector>
#define  TEST_LOG

#ifdef TEST_LOG
#include <iostream>
using namespace std;

#define LOG(stmt) \
{ \
stmt; \
}
#else 

#define LOG(stmt)
#endif


namespace sqlite3_cipher
{
using namespace v8;


typedef enum THREAD_OPERATE_TYPE
{
	THREAD_OPERATE_CONNECT,
	THREAD_OPERATE_OPENDB,
	THREAD_OPERATE_PREPARE_SQL,
	THREAD_OPERATE_EXEC_SQL,
	THREAD_OPERATE_CLOSE_DB,
} THREAD_OPERATE_TYPE;

typedef struct THREAD_INFO
{
	THREAD_OPERATE_TYPE type;
	uv_work_t           req;
	std::string          error;
};
typedef struct CONNECT_THREAD_INFO : public THREAD_INFO
{
	Persistent<Function> callback;
	sqlite3*             pDb;
	std::string          dbpath;
	std::string          key;
} CONNECT_THREAD_INFO;

typedef struct CLOSE_THREAD_INFO : public THREAD_INFO
{
	Persistent<Function> callback;
	sqlite3*             pDb;
};

static void thread_ing(uv_work_t* req);
static void thread_done(uv_work_t* req, int status);

Persistent<Function> Sqlite3CipherDb::constructor;
Persistent<Function> Sqlite3CipherStmt::constructor;



Sqlite3CipherStmt::Sqlite3CipherStmt(Sqlite3CipherDb* pDB, std::string sql) : m_pDB(pDB), m_pStmts(NULL), m_strSql(sql)
{
	if (m_pDB != NULL){
		sqlite3_prepare(m_pDB->m_pDB, m_strSql.c_str(), -1, &m_pStmts, NULL);
	}
}

Sqlite3CipherStmt::~Sqlite3CipherStmt()
{

}
void Sqlite3CipherStmt::Init()
{
	Isolate* isolate = Isolate::GetCurrent();
	Local<FunctionTemplate> tpl = FunctionTemplate::New(isolate, New);
	tpl->SetClassName(String::NewFromUtf8(isolate, "sqlite3ciperhstmt"));
	tpl->InstanceTemplate()->SetInternalFieldCount(2);

	NODE_SET_PROTOTYPE_METHOD(tpl, "bind", bind);
	NODE_SET_PROTOTYPE_METHOD(tpl, "exec", exec);

	constructor.Reset(isolate, tpl->GetFunction());
}

void Sqlite3CipherStmt::New(const FunctionCallbackInfo<Value>& args)
{
	Isolate* isolate = Isolate::GetCurrent();
	HandleScope scope(isolate);

	Sqlite3CipherDb* db = ObjectWrap::Unwrap<Sqlite3CipherDb>(args[0]->ToObject());
	if (db == NULL){
		isolate->ThrowException(String::NewFromUtf8(isolate, "params is wrong."));
		return;
	}
	String::Utf8Value utfSql(args[1]->ToString());

	if (args.IsConstructCall()){;
		Sqlite3CipherStmt* pStmts = new Sqlite3CipherStmt(db,*utfSql);
		pStmts->Wrap(args.This());
		args.GetReturnValue().Set(args.This());
	}
	else {
		args.GetReturnValue().Set(prepare(args[0]->ToObject(isolate), args[1]->ToString()));
	}
}

Local<Object> Sqlite3CipherStmt::prepare(Local<Object> db, Local<String> sql)
{
	Isolate* isolate = Isolate::GetCurrent();
	HandleScope scope(isolate);

	Local<Function> cons = Local<Function>::New(isolate, constructor);
	Local<Context> context = isolate->GetCurrentContext();
	Local<Value>   argv[] = { db, sql };
	Local<Object> instance = cons->NewInstance(context, _countof(argv), argv).ToLocalChecked();

	return instance;
}

void Sqlite3CipherStmt::bind(const FunctionCallbackInfo<Value>& args)
{

}

void Sqlite3CipherStmt::exec(const FunctionCallbackInfo<Value>& args)
{
}





Sqlite3CipherDb::Sqlite3CipherDb()
{
    m_pDB = NULL;
}
Sqlite3CipherDb::~Sqlite3CipherDb()
{
}

void Sqlite3CipherDb::Init()
{
    // 准备构造函数
	Isolate* isolate = Isolate::GetCurrent();
    Local<FunctionTemplate> tpl = FunctionTemplate::New(isolate, New);
    tpl->SetClassName(String::NewFromUtf8(isolate, "sqlite3cipher"));
    tpl->InstanceTemplate()->SetInternalFieldCount(3); // 两个函数

    NODE_SET_PROTOTYPE_METHOD(tpl, "exec", exec);
    NODE_SET_PROTOTYPE_METHOD(tpl, "close", close);
	NODE_SET_PROTOTYPE_METHOD(tpl, "prepare", prepare);
    
    constructor.Reset(isolate, tpl->GetFunction());
}

void Sqlite3CipherDb::New(const FunctionCallbackInfo<Value>& args)
{
    if (args.IsConstructCall()){
        Sqlite3CipherDb* obj = new Sqlite3CipherDb();
        obj->Wrap(args.This());
        args.GetReturnValue().Set(args.This());
    } else {
		Isolate* isolate = args.GetIsolate();
		HandleScope scope(isolate);

		const int argc = 3;
		Local<Value> argv[argc] = {args[0], args[1], args[2]};

		Local<Function> cons = Local<Function>::New(isolate, constructor);
		Local<Context> context = isolate->GetCurrentContext();
		Local<Object> instance = cons->NewInstance(context, argc, argv).ToLocalChecked();
		args.GetReturnValue().Set(instance);
    }
}
void Sqlite3CipherDb::connect(const FunctionCallbackInfo<Value>& args)
{
	Isolate* isolate = args.GetIsolate();
	HandleScope scope(isolate);

	if (!args[0]->IsString())return;

	CONNECT_THREAD_INFO* connectThread = new CONNECT_THREAD_INFO();
	connectThread->type = THREAD_OPERATE_CONNECT;
	connectThread->req.data = connectThread;
	connectThread->pDb = NULL;
	connectThread->dbpath = *(String::Utf8Value(args[0]));

	for (int i = 1; i < 3 && i < args.Length(); ++i){
		if (args[i]->IsFunction()){
			Local<Function> callback = Local<Function>::Cast(args[i]);
			connectThread->callback.Reset(isolate, callback);
		}
		else if (args[i]->IsString()){
			connectThread->key = *(String::Utf8Value(args[i]));
		}
	}
	uv_queue_work(uv_default_loop(), &connectThread->req, (uv_work_cb)&thread_ing, (uv_after_work_cb)&thread_done);
}

void Sqlite3CipherDb::close(const FunctionCallbackInfo<Value>& args)
{
	Isolate* isolate = args.GetIsolate();
	HandleScope scope(isolate);

	Sqlite3CipherDb* sqliteDb = node::ObjectWrap::Unwrap<Sqlite3CipherDb>(args.Holder());

	CLOSE_THREAD_INFO* threadInfo = new CLOSE_THREAD_INFO();
	threadInfo->type = THREAD_OPERATE_CLOSE_DB;
	threadInfo->pDb = sqliteDb->m_pDB;
	threadInfo->req.data = threadInfo;
	
	if (args[0]->IsFunction()){
		Local<Function> callback = Local<Function>::Cast(args[0]);
		threadInfo->callback.Reset(isolate,callback);
	}

	uv_queue_work(uv_default_loop(), &threadInfo->req, (uv_work_cb)&thread_ing, (uv_after_work_cb)&thread_done);
}


void Sqlite3CipherDb::prepare(const FunctionCallbackInfo<Value>& args)
{
	//Local<Sqlite3CipherStmt> inst = Sqlite3CipherStmt::prepare(args.Holder(), args[1]->ToString());
	//callback->Call(isolate->GetCurrentContext()->Global(), _countof(argv), argv);
}

void Sqlite3CipherDb::exec(const FunctionCallbackInfo<Value>& args)
{

}

void thread_ing(uv_work_t* req)
{
	THREAD_INFO* threadInfo = (THREAD_INFO*)req->data;

	if (threadInfo->type == THREAD_OPERATE_CONNECT){
		// 连接数据库
		CONNECT_THREAD_INFO* pConnectInfo = (CONNECT_THREAD_INFO*)req->data;
		if (sqlite3_open(pConnectInfo->dbpath.c_str(), &pConnectInfo->pDb) != SQLITE_OK){
			pConnectInfo->error = "cannot open db.";
			return;
		}
		if (!pConnectInfo->key.empty() &&
			sqlite3_key(pConnectInfo->pDb, (const void*)pConnectInfo->key.c_str(), pConnectInfo->key.size()) != SQLITE_OK){
			pConnectInfo->error = "key is not right.";
			return;
		}
	}
	else if (threadInfo->type == THREAD_OPERATE_CLOSE_DB){
		CLOSE_THREAD_INFO* pCloseInfo = (CLOSE_THREAD_INFO*)req->data;
		if (sqlite3_close(pCloseInfo->pDb) != SQLITE_OK){
			pCloseInfo->error = "cannot close.";
		}
	}
/*
		if (callback->IsNull())return;
		Local<Function> cons = Local<Function>::New(isolate, Sqlite3CipherDb::constructor);
		Local<Context> context = isolate->GetCurrentContext();
		Local<Object> instance = cons->NewInstance(context).ToLocalChecked();
		Sqlite3CipherDb* db = node::ObjectWrap::Unwrap<Sqlite3CipherDb>(instance);
		if (db != NULL){
			db->m_pDB = pDb;
		}
		(CallbackHelper(callback) % Null(isolate) % instance).Callback();
	}
	else if (THREAD_OPERATE_CLOSE_DB == threadInfo->threadType){
		Local<Value> owner = Local<Value>::New(isolate, threadInfo->owner);
		Sqlite3CipherDb* db = node::ObjectWrap::Unwrap<Sqlite3CipherDb>(owner->ToObject());
		Local<Function> callback = Local<Function>::Cast(args->Get(0));
		if (db != NULL && sqlite3_close(db->m_pDB) != SQLITE_OK){
			(CallbackHelper(callback) % Exception::Error(String::NewFromUtf8(isolate, "cannot close db"))).Callback();
		}
		else {
			(CallbackHelper(callback) % Null(isolate)).Callback();
		}
	}
	cout << "thread ing ...." << endl;*/
}

void thread_done(uv_work_t* req, int status)
{
	Isolate* isolate = Isolate::GetCurrent();
	HandleScope scope(isolate);

	THREAD_INFO* threadInfo = (THREAD_INFO*)req->data;

	if (threadInfo->type == THREAD_OPERATE_CONNECT){
		CONNECT_THREAD_INFO* pConnectInfo = (CONNECT_THREAD_INFO*)req->data;
		Local<Function> callback = Local<Function>::New(isolate, pConnectInfo->callback);
		if (!callback->IsFunction())return;
		if (!pConnectInfo->error.empty()){
			Local<Value> argv[] = { Exception::Error(String::NewFromUtf8(isolate, pConnectInfo->error.c_str())) };
			callback->Call(isolate->GetCurrentContext()->Global(), _countof(argv), argv);
		}
		Local<Function> cons = Local<Function>::New(isolate, Sqlite3CipherDb::constructor);
		Local<Context> context = isolate->GetCurrentContext();
		Local<Object> instance = cons->NewInstance(context).ToLocalChecked();
		Sqlite3CipherDb* db = node::ObjectWrap::Unwrap<Sqlite3CipherDb>(instance);
		if (db != NULL){
			db->m_pDB = pConnectInfo->pDb;
		}
		Local<Value> argv[] = { Null(isolate), instance };
		callback->Call(isolate->GetCurrentContext()->Global(), _countof(argv), argv);
	}
	else if (threadInfo->type == THREAD_OPERATE_CLOSE_DB){
		CLOSE_THREAD_INFO* pCloseInfo = (CLOSE_THREAD_INFO*)req->data;
		Local<Function> callback = Local<Function>::New(isolate, pCloseInfo->callback);
		if (!callback->IsFunction())return;
		if (!pCloseInfo->error.empty()){
			Local<Value> argv[] = { Exception::Error(String::NewFromUtf8(isolate, pCloseInfo->error.c_str())) };
			callback->Call(isolate->GetCurrentContext()->Global(), _countof(argv), argv);
		}
		Local<Value> argv[] = { Null(isolate) };
		callback->Call(isolate->GetCurrentContext()->Global(), _countof(argv), argv);
	}
	delete threadInfo;
}

}

