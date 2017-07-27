#include "sqlite3cipherdb.h"

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


Sqlite3CipherStmt::Sqlite3CipherStmt(Local<Sqlite3CipherDb> db) : m_pStmts(NULL)
{
	m_db.Reset(Isolate::GetCurrent(), db);
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

	Local<Sqlite3CipherDb> db = Local<Sqlite3CipherDb>::Cast(args[0]);
	if (db.IsEmpty()){
		isolate->ThrowException(String::NewFromUtf8(isolate, "params is wrong."));
		return;
	}

	if (args.IsConstructCall()){

	}
}
struct EXEC_SQL_WORKER
{
	uv_work_t request;
	Persistent<Function> callback;
	Persistent<Object>   execObj; // 操作体
	Persistent<Value>    callbackResults;

	std::string strErrStr;
	Sqlite3CipherDb* pSqliteCiper;
};

Persistent<Function> Sqlite3CipherDb::constructor;



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
    tpl->InstanceTemplate()->SetInternalFieldCount(2); // 两个函数

    NODE_SET_PROTOTYPE_METHOD(tpl, "exec", exec);
    NODE_SET_PROTOTYPE_METHOD(tpl, "close", close);
    
    constructor.Reset(isolate, tpl->GetFunction());
}

void Sqlite3CipherDb::New(const FunctionCallbackInfo<Value>& args)
{
    if (args.IsConstructCall()){
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
	Isolate* isolate = args.GetIsolate();
	HandleScope scope(isolate);

    const int argc = 3;
    Local<Value> argv[argc] = {args[0], args[1], args[2]};

	Local<Function> cons = Local<Function>::New(isolate, constructor);
    Local<Context> context = isolate->GetCurrentContext();
    Local<Object> instance =
        cons->NewInstance(context, argc, argv).ToLocalChecked();
    args.GetReturnValue().Set(instance);
}

void Sqlite3CipherDb::connect(const FunctionCallbackInfo<Value>& args)
{
	Isolate* isolate = args.GetIsolate();
	HandleScope scope(isolate);

	if (args.Length() < 1){ return; }

	LOG(std::cout << "connect ... " << std::endl;)

	sqlite3* pDB = NULL;
	std::string strDbPath = *(String::Utf8Value(args[0]->ToString()));
	std::string strKey;
	Local<Function> callback;

	LOG(cout << "args.length : " << args.Length() << endl;)
	for (int i = 1; i < 3 && i < args.Length(); ++i){
		if (args[i]->IsFunction()){
			cout << "args [" << i << "]" << "is function." << endl;
			callback = Local<Function>::Cast(args[i]);
		}
		else if (args[i]->IsString()){
			cout << "args [" << i << "]" << "is string." << endl;
			strKey = *(String::Utf8Value(args[i]->ToString()));
		}
	}
	if (sqlite3_open(strDbPath.c_str(), &pDB) != SQLITE_OK){
		if (callback->IsNull())return;
		Local<Value> argv[] = { Exception::Error(String::NewFromUtf8(isolate, "cannot open db")) };
		callback->Call(isolate->GetCurrentContext()->Global(), _countof(argv), argv);
		return;
	}
	if (!strKey.empty() && sqlite3_key(pDB, (const void*)strKey.c_str(), strKey.size()) != SQLITE_OK){
		if (callback->IsNull())return;
		Local<Value> argv[] = { Exception::Error(String::NewFromUtf8(isolate, "key is wrong")) };
		callback->Call(isolate->GetCurrentContext()->Global(), _countof(argv), argv);
		return;
	}
	LOG(cout << "dbpath : " << strDbPath.c_str() << endl;)
	LOG(cout << "key : " << strKey.c_str() << endl;)
	if (callback->IsNull())return;
	Local<Function> cons = Local<Function>::New(isolate, Sqlite3CipherDb::constructor);
	Local<Context> context = isolate->GetCurrentContext();
	Local<Object> instance =
		cons->NewInstance(context).ToLocalChecked();

	Sqlite3CipherDb* pSqlite3Db = ObjectWrap::Unwrap<Sqlite3CipherDb>(instance);
	pSqlite3Db->m_pDB = pDB;

	Local<Value> argv[] = { Null(isolate), instance };

	callback->Call(isolate->GetCurrentContext()->Global(), _countof(argv), argv);
}

void Sqlite3CipherDb::exec(const FunctionCallbackInfo<Value>& args)
{
	Isolate* isolate = args.GetIsolate();
	HandleScope scope(isolate);

	if (args.Length() < 1){
		isolate->ThrowException(String::NewFromUtf8(isolate, "params error."));
		return;
	}
	EXEC_SQL_WORKER* worker = new EXEC_SQL_WORKER;
	worker->request.data = worker;
	worker->execObj.Reset(isolate,args[0]->ToObject());
	if (args.Length() > 1){
		Local<Function> cb = Local<Function>::Cast(args[1]);
		if (!cb->IsFunction()){
			isolate->ThrowException(String::NewFromUtf8(isolate, "params error."));
			return;
		}
		worker->callback.Reset(isolate, cb);
	}
	Sqlite3CipherDb* pOwner = node::ObjectWrap::Unwrap<Sqlite3CipherDb>(args.Holder());
	worker->pSqliteCiper = pOwner;
	uv_queue_work(uv_default_loop(), &worker->request, (uv_work_cb)&Sqlite3CipherDb::exec_ing_, (uv_after_work_cb)&Sqlite3CipherDb::exec_done_);
}

void Sqlite3CipherDb::close(const FunctionCallbackInfo<Value>& args)
{
	Isolate* isolate = args.GetIsolate();
	HandleScope scope(isolate);

	Sqlite3CipherDb* pOwner = node::ObjectWrap::Unwrap<Sqlite3CipherDb>(args.Holder());
	sqlite3_close(pOwner->m_pDB);
	pOwner->m_pDB = NULL;
}

void Sqlite3CipherDb::exec_ing_(uv_work_t* req)
{
	EXEC_SQL_WORKER* worker = static_cast<EXEC_SQL_WORKER*>(req->data);

	Isolate* isolate = Isolate::GetCurrent();
	HandleScope scope(isolate);

	// 确定是不是查询操作
	Local<Object> execObj = Local<Object>::New(isolate, worker->execObj);
	Local<Value> query = execObj->Get(String::NewFromUtf8(isolate, "query"));
	Local<Value> insert = execObj->Get(String::NewFromUtf8(isolate, "insert"));
	Local<Value> update = execObj->Get(String::NewFromUtf8(isolate, "update"));
	Local<Value> delobj = execObj->Get(String::NewFromUtf8(isolate, "del"));

	if (query->IsObject()){
		Local<Object> queryObj = query->ToObject();
		// 执行查询
		// 1. 取得tablename;
		// 2. 判断是否有count ?
		// 3. 判断是否有limit
		// 4. 判断是否有skip
		// 5. 解析条件
		Local<Value> tableName = queryObj->Get(String::NewFromUtf8(isolate,"tablename"));
		Local<Value> count = queryObj->Get(String::NewFromUtf8(isolate, "count"));
		Local<Value> skip = queryObj->Get(String::NewFromUtf8(isolate, "skip"));
		Local<Value> limit = queryObj->Get(String::NewFromUtf8(isolate, "limit"));
		Local<Value> sort = queryObj->Get(String::NewFromUtf8(isolate, "sort"));
		Local<Value> condition = queryObj->Get(String::NewFromUtf8(isolate, "condition"));

		if (!condition->IsObject()){
			worker->strErrStr = "query object is not right.";
			return;
		}
		// 解析查询条件
		std::string strCondition;
		std::vector<ParamNode*> params;
		bool bIsError = false;
		if (!Expression(condition, strCondition, params)){
			worker->strErrStr = "query object is error";
			bIsError = true;
		}
		if (!bIsError){
			std::string strSql = "SELECT * FROM ";
			if (!count->IsUndefined()){
				strSql = "SELECT COUNT(*) FROM ";
			}
			if (!tableName->IsString()){
				worker->strErrStr = "tableName is not right.";
				bIsError = true;
			}
			else {
				strSql += *String::Utf8Value(tableName->ToString(isolate));
				if (!strCondition.empty()){
					strSql += " WHERE " + strCondition;
				}
				SortExpression(sort, strSql, params);
				if (limit->IsInt32()){
					char szLimit[20];
					sprintf_s(szLimit, sizeof(szLimit), "%d", limit->Int32Value());
					strSql += " LIMIT ";
					strSql += szLimit;
				}
				if (skip->IsInt32()){
					char szSkip[20];
					sprintf_s(szSkip, sizeof(szSkip), "%d", skip->Int32Value());
					strSql += " OFFSET ";
					strSql += szSkip;
				}

				// 完成了准备工作，将sql绑定到sqlite3中
				sqlite3_stmt* stmt = NULL;
				if (sqlite3_prepare_v2(worker->pSqliteCiper->m_pDB, strSql.c_str(),
					-1, &stmt, NULL) != SQLITE_OK){
					worker->strErrStr = "failed.";
				}
				else {
					for (size_t i = 0; i < params.size(); ++i){
						switch (params[i]->type){
						case PARAM_INT:sqlite3_bind_int64(stmt, i, params[i]->integer); break;
						case PARAM_BOOL:sqlite3_bind_int(stmt, i, params[i]->boolVal); break;
						case PARAM_DOUBLE:sqlite3_bind_double(stmt, i, params[i]->lfFloat); break;
						case PARAM_TEXT:sqlite3_bind_text(stmt, i, params[i]->strText.c_str(), -1, SQLITE_TRANSIENT); break;
						case PARAM_BLOB:sqlite3_bind_blob(stmt, i, params[i]->pBlob, params[i]->blobbytes, NULL); break;
						}
					}

					// 执行
					int ret = SQLITE_OK;
					bool bWillCollect = worker->callback.IsEmpty() == false;
					while (true){
						ret = sqlite3_step(stmt);
						if (ret == SQLITE_ROW && bWillCollect){
						}
						if (ret == SQLITE_DONE)break;
						if (ret == SQLITE_ERROR)break;
					}
				}
			}
		}

		for (size_t i = 0; i < params.size(); ++i){
			delete params[i];
		}
		return;
	}
}

void Sqlite3CipherDb::exec_done_(uv_work_t* req, int status)
{
	Isolate* isolate = Isolate::GetCurrent();
	HandleScope scope(isolate);

	EXEC_SQL_WORKER* worker = static_cast<EXEC_SQL_WORKER*>(req->data);

	if (!worker->callback.IsEmpty()){
		Local<Function> cb = Local<Function>::New(isolate, worker->callback);
		const int argc = 0;
		Local<Value> argv[] = { Undefined(isolate) };
		cb->Call(isolate->GetCurrentContext()->Global(), argc, argv);
	}

	delete worker;
}

}

