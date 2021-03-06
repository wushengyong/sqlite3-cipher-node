#include "sqlite3cipherdb.h"
#include <vector>
#include <map>
#include "sqlite3.h"
#include <limits.h>
#include <node_buffer.h>

namespace sqlite3_cipher
{
	using namespace v8;


	typedef enum THREAD_OPERATE_TYPE
	{
		THREAD_OPERATE_CONNECT,
		THREAD_OPERATE_PREPARE_SQL,
		THREAD_OPERATE_EXEC_SQL,
		THREAD_OPERATE_CLOSE_DB,
		THREAD_OPERATE_EXEC_PREPARE_STMTS
	} THREAD_OPERATE_TYPE;


	typedef struct THREAD_INFO
	{
		THREAD_OPERATE_TYPE  type;
		uv_work_t            req;
		std::string          error;
		Persistent<Function> callback;
		sqlite3*             pDb;

		virtual ~THREAD_INFO(){}
	} THREAD_INFO;
	typedef struct CONNECT_THREAD_INFO : public THREAD_INFO
	{
		std::string          dbpath;
		std::string          key;
	} CONNECT_THREAD_INFO;

	typedef struct CLOSE_THREAD_INFO : public THREAD_INFO
	{
	} CLOSE_THREAD_INFO;


	typedef struct RESULT_INFO
	{
		int type;
		unsigned char* data;
		int bytes;
	} RESULT_INFO;

	typedef std::vector<std::map<std::string, RESULT_INFO*>> RESULT_TYPE;

	typedef struct EXEC_THREAD_INFO : public THREAD_INFO
	{
		std::string strSql;
		RESULT_TYPE results;
	} EXEC_THREAD_INFO;
	


	typedef struct PREPARE_THREAD_INFO : public THREAD_INFO
	{
		std::string        strSql;
		sqlite3_stmt*      stmt;
	} PREPARE_THREAD_INFO;

	typedef struct EXEC_PREPARE_STMT_THREAD_INFO : public THREAD_INFO
	{
		sqlite3_stmt*      stmt;
		RESULT_TYPE        results;
	} EXEC_PREPARE_STMT_THREAD_INFO;


	Persistent<Function> Sqlite3CipherDb::constructor;
	Persistent<Function> Sqlite3CipherStmt::constructor;

	class GlobalMutex
	{
	public:
		static GlobalMutex* GetMutex() { return &globalMutex; }
		~GlobalMutex() { ::DeleteCriticalSection(&cs); }
		LPCRITICAL_SECTION GetCriticalSection() { return &cs; }
	private:
		GlobalMutex() { ::InitializeCriticalSection(&cs); }
	private:
		static GlobalMutex globalMutex;
		CRITICAL_SECTION cs;
	};
	GlobalMutex GlobalMutex::globalMutex;

	class CAutoMutex
	{
	public:
		CAutoMutex(){ ::EnterCriticalSection(GlobalMutex::GetMutex()->GetCriticalSection()); }
		~CAutoMutex() { ::LeaveCriticalSection(GlobalMutex::GetMutex()->GetCriticalSection()); }
	};


	RESULT_INFO* MallocResultInfo(int type, const unsigned char* data, int bytes)
	{
		RESULT_INFO* pResultInfo = new RESULT_INFO;

		pResultInfo->type = type;
		if (bytes == 0){
			pResultInfo->data = NULL;
			pResultInfo->bytes = 0;
			return pResultInfo;
		}
		else if (bytes < 0){
			pResultInfo->bytes = strlen((char*)data) + 1;
		}
		else {
			pResultInfo->bytes = bytes;
		}
		pResultInfo->data = new unsigned char[pResultInfo->bytes];
		
		memcpy(pResultInfo->data, data, pResultInfo->bytes);
		return pResultInfo;
	}
	void FreeResultInfo(RESULT_INFO* pResult)
	{
		if (pResult == NULL)return;
		if (pResult->data != NULL){
			delete[] pResult->data;
		}
		delete pResult;
	}

	Local<Value> GetValueFromResult(RESULT_INFO* pResult)
	{
		Isolate* isolate = Isolate::GetCurrent();
		// HandleScope scope(isolate);，不能加上这句，否则返回的内容将会变为无效
		if (SQLITE_INTEGER == pResult->type){
			__int64 i64Value = *(__int64*)pResult->data;
			if (i64Value > INT_MAX){
				return Number::New(isolate,(double)i64Value);
			}
			return Integer::New(isolate,(int)i64Value);
		}
		else if (SQLITE_FLOAT == pResult->type){
			double lfValue = *(double*)pResult->data;
			return Number::New(isolate, *(double*)pResult->data);
		}
		else if (SQLITE_BLOB == pResult->type){
			return node::Buffer::New(isolate, (char*)pResult->data, pResult->bytes).ToLocalChecked();
		}
		else if (SQLITE_NULL == pResult->type){
			return Null(isolate);
		}
		else if (SQLITE3_TEXT == pResult->type){
			return String::NewFromUtf8(isolate, (const char*)pResult->data);
		}
		return Null(isolate);
	}

	RESULT_INFO* GetStmtColInfo(sqlite3_stmt* pStmt, int nColIndex)
	{
		if (pStmt == NULL)return NULL;

		int type = sqlite3_column_type(pStmt, nColIndex);

		if (SQLITE_INTEGER == type){
			__int64 nValue = sqlite3_column_int64(pStmt, nColIndex);
			return MallocResultInfo(type, (const unsigned char*)&nValue, sizeof(nValue));
		}
		else if (SQLITE_FLOAT == type){
			double lfValue = sqlite3_column_double(pStmt, nColIndex);
			return MallocResultInfo(type, (const unsigned char*)&lfValue, sizeof(lfValue));
		}
		else if (SQLITE_BLOB == type){
			const char* buff = (const char*)sqlite3_column_blob(pStmt, nColIndex);
			return MallocResultInfo(type, (const unsigned char*)sqlite3_column_blob(pStmt, nColIndex), sqlite3_column_bytes(pStmt, nColIndex));
		}
		else if (SQLITE3_TEXT == type){
			return MallocResultInfo(type, (const unsigned char*)sqlite3_column_text(pStmt, nColIndex), -1);
		}
		return NULL;
	}

	Sqlite3CipherStmt::Sqlite3CipherStmt() : m_pDB(NULL), m_pStmts(NULL)
	{
	}

	Sqlite3CipherStmt::~Sqlite3CipherStmt()
	{
		if (m_pStmts){
			sqlite3_finalize(m_pStmts);
		}
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
		if (args.IsConstructCall()){
			Sqlite3CipherStmt* obj = new Sqlite3CipherStmt();
			obj->Wrap(args.This());
			args.GetReturnValue().Set(args.This());
		}
		else {
			Isolate* isolate = args.GetIsolate();
			HandleScope scope(isolate);

			const int argc = 3;
			Local<Value> argv[argc] = { args[0], args[1], args[2] };

			Local<Function> cons = Local<Function>::New(isolate, constructor);
			Local<Context> context = isolate->GetCurrentContext();
			Local<Object> instance = cons->NewInstance(context, argc, argv).ToLocalChecked();
			args.GetReturnValue().Set(instance);
		}
	}
	void Sqlite3CipherStmt::bind(const FunctionCallbackInfo<Value>& args)
	{
		Isolate* isolate = Isolate::GetCurrent();
		HandleScope scope(isolate);

		Sqlite3CipherStmt* pStmt = node::ObjectWrap::Unwrap<Sqlite3CipherStmt>(args.Holder());

		if (!args[0]->IsInt32()){
			isolate->ThrowException(String::NewFromUtf8(isolate, "args[0] must be an integer."));
			return;
		}
		int nCol = args[0]->Int32Value() + 1;

		Local<Value> value = args[1];

		bool bRet = false;
		if (value->IsInt32()){
			bRet = sqlite3_bind_int(pStmt->m_pStmts, nCol, value->Int32Value()) == SQLITE_OK;
		}
		else if (value->IsUint32()){
			bRet = sqlite3_bind_int64(pStmt->m_pStmts, nCol, (__int64)value->Uint32Value()) == SQLITE_OK;
		}
		else if (value->IsBoolean()){
			bRet = sqlite3_bind_int(pStmt->m_pStmts, nCol, value->BooleanValue() ? 1 : 0) == SQLITE_OK;
		}
		else if (value->IsNumber()){
			bRet = sqlite3_bind_double(pStmt->m_pStmts, nCol, value->NumberValue()) == SQLITE_OK;
		}
		else if (value->IsArrayBufferView()){
			char* buff = node::Buffer::Data(value);
			size_t size = node::Buffer::Length(value);
			bRet = sqlite3_bind_blob(pStmt->m_pStmts, nCol, (const void*)buff, size, SQLITE_TRANSIENT) == SQLITE_OK;
		}
		else if (value->IsString()){
			std::string text = *(String::Utf8Value(value->ToString()));
			bRet = sqlite3_bind_text(pStmt->m_pStmts, nCol, text.c_str(), -1, SQLITE_TRANSIENT) == SQLITE_OK;
		}
		else
		{
			isolate->ThrowException(String::NewFromUtf8(isolate, "cannot bind this type"));
		}

		args.GetReturnValue().Set(Boolean::New(isolate,bRet));
	}

	void Sqlite3CipherStmt::exec(const FunctionCallbackInfo<Value>& args)
	{
		Isolate* isolate = Isolate::GetCurrent();
		HandleScope scope(isolate);


		Sqlite3CipherStmt* pStmt = node::ObjectWrap::Unwrap<Sqlite3CipherStmt>(args.Holder());

		EXEC_PREPARE_STMT_THREAD_INFO* pExecPrepareStmtInfo = new EXEC_PREPARE_STMT_THREAD_INFO();
		pExecPrepareStmtInfo->type = THREAD_OPERATE_EXEC_PREPARE_STMTS;
		pExecPrepareStmtInfo->req.data = pExecPrepareStmtInfo;
		pExecPrepareStmtInfo->pDb = pStmt->m_pDB;
		pExecPrepareStmtInfo->stmt = pStmt->m_pStmts;
		pStmt->m_pStmts = NULL;

		if (args[0]->IsFunction()){
			Local<Function> callback = Local<Function>::Cast(args[0]);
			pExecPrepareStmtInfo->callback.Reset(isolate, callback);
		}
		uv_queue_work(uv_default_loop(), &pExecPrepareStmtInfo->req, (uv_work_cb)&ThreadFunctions::thread_ing, (uv_after_work_cb)&ThreadFunctions::thread_done);
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
		}
		else {
			Isolate* isolate = args.GetIsolate();
			HandleScope scope(isolate);

			const int argc = 3;
			Local<Value> argv[argc] = { args[0], args[1], args[2] };

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

		if (!args[0]->IsString()){
			isolate->ThrowException(String::NewFromUtf8(isolate, "args[0] must be an string."));
			return;
		}

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
		uv_queue_work(uv_default_loop(), &connectThread->req, (uv_work_cb)&ThreadFunctions::thread_ing, (uv_after_work_cb)&ThreadFunctions::thread_done);
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
			threadInfo->callback.Reset(isolate, callback);
		}

		uv_queue_work(uv_default_loop(), &threadInfo->req, (uv_work_cb)&ThreadFunctions::thread_ing, (uv_after_work_cb)&ThreadFunctions::thread_done);
	}


	void Sqlite3CipherDb::prepare(const FunctionCallbackInfo<Value>& args)
	{
		Isolate* isolate = args.GetIsolate();
		HandleScope scope(isolate);

		Sqlite3CipherDb* sqlitedb = node::ObjectWrap::Unwrap<Sqlite3CipherDb>(args.Holder());

		PREPARE_THREAD_INFO* threadInfo = new PREPARE_THREAD_INFO();
		threadInfo->type = THREAD_OPERATE_PREPARE_SQL;
		threadInfo->pDb = sqlitedb->m_pDB;
		threadInfo->req.data = threadInfo;

		if (args[0]->IsString()){
			threadInfo->strSql = *(String::Utf8Value(args[0]));
		}

		if (args[1]->IsFunction()){
			Local<Function> callback = Local<Function>::Cast(args[1]);
			threadInfo->callback.Reset(isolate, callback);
		}
		uv_queue_work(uv_default_loop(), &threadInfo->req, (uv_work_cb)&ThreadFunctions::thread_ing, (uv_after_work_cb)&ThreadFunctions::thread_done);
	}

	void Sqlite3CipherDb::exec(const FunctionCallbackInfo<Value>& args)
	{
		Isolate* isolate = args.GetIsolate();
		HandleScope scope(isolate);

		Sqlite3CipherDb* sqlitedb = node::ObjectWrap::Unwrap<Sqlite3CipherDb>(args.Holder());

		EXEC_THREAD_INFO* threadInfo = new EXEC_THREAD_INFO();
		threadInfo->type = THREAD_OPERATE_EXEC_SQL;
		threadInfo->pDb = sqlitedb->m_pDB;
		threadInfo->req.data = threadInfo;

		if (args[0]->IsString()){
			threadInfo->strSql = *(String::Utf8Value(args[0]));
		}
		if (args[1]->IsFunction()){
			Local<Function> callback = Local<Function>::Cast(args[1]);
			threadInfo->callback.Reset(isolate, callback);
		}

		uv_queue_work(uv_default_loop(), &threadInfo->req, (uv_work_cb)&ThreadFunctions::thread_ing, (uv_after_work_cb)&ThreadFunctions::thread_done);
	}
	int collect_results_from_stmts(sqlite3_stmt* pStmt, RESULT_TYPE& results)
	{
		int nCols = sqlite3_column_count(pStmt);
		int rc = SQLITE_OK;
		while (1){

			rc = sqlite3_step(pStmt);
			if (rc == SQLITE_ROW && nCols > 0){
				results.push_back(RESULT_TYPE::value_type());
				RESULT_TYPE::value_type& mapRow = results.back();
				for (int i = 0; i < nCols; ++i){
					mapRow[sqlite3_column_name(pStmt, i)] = GetStmtColInfo(pStmt, i);
				}
			}
			if (rc != SQLITE_ROW){
				break;
			}
		}
		rc = sqlite3_finalize(pStmt);
		return rc;
	}
	bool collect_results(sqlite3* db, std::string sql, RESULT_TYPE& results)
	{
		int rc = SQLITE_OK;         /* Return code */
		const char *zLeftover;      /* Tail of unprocessed SQL */
		sqlite3_stmt *pStmt = 0;    /* The current SQL statement */
		const char* zSql = sql.c_str();

		while (rc == SQLITE_OK && zSql[0]){
			char **azVals = 0;

			pStmt = 0;
			rc = sqlite3_prepare_v2(db, zSql, -1, &pStmt, &zLeftover);
			assert(rc == SQLITE_OK || pStmt == 0);
			if (rc != SQLITE_OK){
				continue;
			}
			if (!pStmt){
				/* this happens for a comment or white-space */
				zSql = zLeftover;
				continue;
			}
			rc = collect_results_from_stmts(pStmt, results);
			pStmt = NULL;
			zSql = zLeftover;
		}
		return rc == SQLITE_OK;
	}


	void ThreadFunctions::thread_ing(uv_work_t* req)
	{
		THREAD_INFO* threadInfo = (THREAD_INFO*)req->data;

		CAutoMutex mutex;

		if (threadInfo->type == THREAD_OPERATE_CONNECT){
			// 连接数据库
			CONNECT_THREAD_INFO* pConnectInfo = (CONNECT_THREAD_INFO*)req->data;
			if (sqlite3_open(pConnectInfo->dbpath.c_str(), &pConnectInfo->pDb) != SQLITE_OK){
				pConnectInfo->error = sqlite3_errmsg(pConnectInfo->pDb);
				return;
			}
			if (!pConnectInfo->key.empty() &&
				sqlite3_key(pConnectInfo->pDb, (const void*)pConnectInfo->key.c_str(), pConnectInfo->key.size() * sizeof(std::string::value_type)) != SQLITE_OK){
				pConnectInfo->error = sqlite3_errmsg(pConnectInfo->pDb);
				return;
			}
		}
		else if (threadInfo->type == THREAD_OPERATE_CLOSE_DB){
			CLOSE_THREAD_INFO* pCloseInfo = (CLOSE_THREAD_INFO*)req->data;
			if (sqlite3_close(pCloseInfo->pDb) != SQLITE_OK){
				pCloseInfo->error = sqlite3_errmsg(pCloseInfo->pDb);
			}
		}
		else if (threadInfo->type == THREAD_OPERATE_EXEC_SQL){
			EXEC_THREAD_INFO* pExeInfo = (EXEC_THREAD_INFO*)req->data;
			if (!collect_results(pExeInfo->pDb, pExeInfo->strSql, pExeInfo->results)){
				pExeInfo->error = sqlite3_errmsg(pExeInfo->pDb);
			}
		}
		else if (threadInfo->type == THREAD_OPERATE_PREPARE_SQL){
			PREPARE_THREAD_INFO* pPrepareInfo = (PREPARE_THREAD_INFO*)req->data;
			if (sqlite3_prepare_v2(pPrepareInfo->pDb, pPrepareInfo->strSql.c_str(), -1, &pPrepareInfo->stmt, NULL) != SQLITE_OK){
				pPrepareInfo->error = sqlite3_errmsg(pPrepareInfo->pDb);
			}
		}
		else if (threadInfo->type == THREAD_OPERATE_EXEC_PREPARE_STMTS){
			EXEC_PREPARE_STMT_THREAD_INFO* pExecPrepareStmtsInfo = (EXEC_PREPARE_STMT_THREAD_INFO*)req->data;
			if (collect_results_from_stmts(pExecPrepareStmtsInfo->stmt, pExecPrepareStmtsInfo->results) != SQLITE_OK){
				pExecPrepareStmtsInfo->error = sqlite3_errmsg(pExecPrepareStmtsInfo->pDb);
			}
		}

	}

	void ThreadFunctions::thread_done(uv_work_t* req, int status)
	{
		Isolate* isolate = Isolate::GetCurrent();
		HandleScope scope(isolate);

		THREAD_INFO* threadInfo = (THREAD_INFO*)req->data;

		if (threadInfo->type == THREAD_OPERATE_CONNECT){
			CONNECT_THREAD_INFO* pConnectInfo = (CONNECT_THREAD_INFO*)req->data;
			Local<Function> callback = Local<Function>::New(isolate, pConnectInfo->callback);
			if (callback->IsFunction()) {
				if (!pConnectInfo->error.empty()){
					Local<Value> argv[] = { Exception::Error(String::NewFromUtf8(isolate, pConnectInfo->error.c_str())) };
					callback->Call(isolate->GetCurrentContext()->Global(), _countof(argv), argv);
				}
				else {
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

			}
			delete pConnectInfo;
		}
		else if (threadInfo->type == THREAD_OPERATE_CLOSE_DB){
			CLOSE_THREAD_INFO* pCloseInfo = (CLOSE_THREAD_INFO*)req->data;
			Local<Function> callback = Local<Function>::New(isolate, pCloseInfo->callback);
			if (callback->IsFunction()){
				if (!pCloseInfo->error.empty()){
					Local<Value> argv[] = { Exception::Error(String::NewFromUtf8(isolate, pCloseInfo->error.c_str())) };
					callback->Call(isolate->GetCurrentContext()->Global(), _countof(argv), argv);
				}
				else {
					Local<Value> argv[] = { Null(isolate) };
					callback->Call(isolate->GetCurrentContext()->Global(), _countof(argv), argv);
				}
			}
			delete pCloseInfo;
		}
		else if (threadInfo->type == THREAD_OPERATE_EXEC_SQL){
			EXEC_THREAD_INFO* pExeThreadInfo = (EXEC_THREAD_INFO*)req->data;
			Local<Function> callback = Local<Function>::New(isolate, pExeThreadInfo->callback);
			if (callback->IsFunction()){
				if (!pExeThreadInfo->error.empty()){
					Local<Value> argv[] = { Exception::Error(String::NewFromUtf8(isolate, pExeThreadInfo->error.c_str())) };
					callback->Call(isolate->GetCurrentContext()->Global(), _countof(argv), argv);
				}
				else {
					Local<Array> results = Array::New(isolate, pExeThreadInfo->results.size());
					for (size_t i = 0; i < pExeThreadInfo->results.size(); ++i){
						RESULT_TYPE::value_type& info = pExeThreadInfo->results[i];
						if (info.size() <= 0)continue;
						Local<Object> obj = Object::New(isolate);
						for (auto itr = info.begin(); itr != info.end(); ++itr){
							if (itr->second == NULL || itr->second->data == NULL)continue;
							obj->Set(String::NewFromUtf8(isolate, itr->first.c_str()), GetValueFromResult(itr->second));
						}
						results->Set(isolate->GetCurrentContext(), i, obj);
					}
					Local<Value> argv[] = { Null(isolate), results };
					callback->Call(isolate->GetCurrentContext()->Global(), _countof(argv), argv);
				}
			}
			for (size_t i = 0; i < pExeThreadInfo->results.size(); ++i){
				RESULT_TYPE::value_type& info = pExeThreadInfo->results[i];
				for (auto itr = info.begin(); itr != info.end(); ++itr){
					FreeResultInfo(itr->second);
				}
			}
			delete pExeThreadInfo;
		}
		else if (threadInfo->type == THREAD_OPERATE_PREPARE_SQL){
			PREPARE_THREAD_INFO* pPrepareInfo = (PREPARE_THREAD_INFO*)req->data;
			Local<Function> callback = Local<Function>::New(isolate, pPrepareInfo->callback);
			if (callback->IsFunction())
			{
				if (!pPrepareInfo->error.empty()){
					Local<Value> argv[] = { Exception::Error(String::NewFromUtf8(isolate, pPrepareInfo->error.c_str())) };
					callback->Call(isolate->GetCurrentContext()->Global(), _countof(argv), argv);
				}
				else {
					Local<Function> cons = Local<Function>::New(isolate, Sqlite3CipherStmt::constructor);
					Local<Context> context = isolate->GetCurrentContext();
					Local<Object> instance = cons->NewInstance(context).ToLocalChecked();
					Sqlite3CipherStmt* stmts = node::ObjectWrap::Unwrap<Sqlite3CipherStmt>(instance);
					if (stmts != NULL){
						stmts->m_pDB = pPrepareInfo->pDb;
						stmts->m_pStmts = pPrepareInfo->stmt;
					}
					Local<Value> argv[] = { Null(isolate), instance };
					callback->Call(isolate->GetCurrentContext()->Global(), _countof(argv), argv);
				}
			}
			delete pPrepareInfo;
		}
		else if (threadInfo->type == THREAD_OPERATE_EXEC_PREPARE_STMTS){
			EXEC_PREPARE_STMT_THREAD_INFO* pExeThreadInfo = (EXEC_PREPARE_STMT_THREAD_INFO*)req->data;
			Local<Function> callback = Local<Function>::New(isolate, pExeThreadInfo->callback);
			if (callback->IsFunction()){
				if (!pExeThreadInfo->error.empty()){
					Local<Value> argv[] = { Exception::Error(String::NewFromUtf8(isolate, pExeThreadInfo->error.c_str())) };
					callback->Call(isolate->GetCurrentContext()->Global(), _countof(argv), argv);
				}
				else {
					Local<Array> results = Array::New(isolate, pExeThreadInfo->results.size());
					for (size_t i = 0; i < pExeThreadInfo->results.size(); ++i){
						RESULT_TYPE::value_type& info = pExeThreadInfo->results[i];
						if (info.size() <= 0)continue;
						Local<Object> obj = Object::New(isolate);
						for (auto itr = info.begin(); itr != info.end(); ++itr){
							if (itr->second == NULL || itr->second->data == NULL)continue;
							obj->Set(String::NewFromUtf8(isolate, itr->first.c_str()), GetValueFromResult(itr->second));
						}
						results->Set(isolate->GetCurrentContext(), i, obj);
					}
					Local<Value> argv[] = { Null(isolate), results };
					callback->Call(isolate->GetCurrentContext()->Global(), _countof(argv), argv);
				}
			}
			for (size_t i = 0; i < pExeThreadInfo->results.size(); ++i){
				RESULT_TYPE::value_type& info = pExeThreadInfo->results[i];
				for (auto itr = info.begin(); itr != info.end(); ++itr){
					FreeResultInfo(itr->second);
				}
			}
			delete pExeThreadInfo;
		}
	}

}