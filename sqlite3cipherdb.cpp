#include "sqlite3cipherdb.h"

namespace sqlite3_cipher
{
using namespace v8;


struct EXEC_SQL_WORKER
{
	uv_work_t request;
	Persistent<Function> callback;
	Persistent<Object>   execObj; // 操作体

	std::string strErrStr;
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

	uv_queue_work(uv_default_loop(), &worker->request, (uv_work_cb)&Sqlite3CipherDb::exec_ing_, (uv_after_work_cb)&Sqlite3CipherDb::exec_done_);
}

void Sqlite3CipherDb::close(const FunctionCallbackInfo<Value>& args)
{

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
typedef bool(*ExpressionFunc)(Local<Value>&, std::string&, std::vector<ParamNode*>&);

bool Sqlite3CipherDb::Expression(Local<Value>& value, std::string& sql, std::vector<ParamNode*>& params)
{
	Isolate* isolate = Isolate::GetCurrent();
	HandleScope scope(isolate);

	if (value->IsArray()){
		return AndExpression(value, sql, params);
	}
	else if (value->IsObject()){
		Local<Object> obj = value->ToObject(isolate);
		const std::string expressionNames[] = { "$or", "$and", "$gt", "$lt", "$gte", "$lte", /*"$exists",*/ "$in", "$nin" };
		const ExpressionFunc expressions[] = { &OrExpression, &AndExpression, &GreatThanExpression, &LessThanExpression,
		&GreatThanEquExpression, &LessThanEquExpression, /*&ExistsExpression,*/ &InExpression, &NotInExpression };
		
		Local<Array> arrayNames = obj->GetOwnPropertyNames();
		if (arrayNames->Length() < 0){
			return false;
		}
		else if(arrayNames->Length() == 1){
			Local<Value> key = arrayNames->Get(0);
			if (key->ToString().IsEmpty()){
				return false;
			}
			String::Utf8Value keyName(key->ToString());
			for (size_t i = 0; i < _countof(expressionNames); ++i){
				if ((*keyName) == expressionNames[i]){
					Local<Value> subexpression = obj->Get(key);
					return (*expressions[i])(subexpression, sql, params);
				}
			}
		}
		// Eq
		return EquExpression(value, sql, params);
	}
	return false;
}

bool Sqlite3CipherDb::OrExpression(Local<Value>& value, std::string& sql, std::vector<ParamNode*>& params)
{
	// 收集所有的信息
	Isolate* isolate = Isolate::GetCurrent();
	HandleScope scope(isolate);

	if (!value->IsArray())return false;

	Local<Object> obj = value->ToObject();
	Local<Array> arrayNames = obj->GetOwnPropertyNames();

	int size = arrayNames->Length();
	if (size < 1)return false;

	sql += " (";
	for (int i = 0; i < size; ++i){
		Local<Value> key = arrayNames->Get(i);
		if (!Expression(obj->Get(key), sql, params)){
			return false;
		}
		if (i != size - 1){
			sql += " OR ";
		}
	}
	sql += ") ";
	return true;

}

bool Sqlite3CipherDb::AndExpression(Local<Value>& value, std::string& sql, std::vector<ParamNode*>& params)
{
	// Collect infos;
	Isolate* isolate = Isolate::GetCurrent();
	HandleScope scope(isolate);

	if (!value->IsArray())return false;

	Local<Object> obj = value->ToObject();
	Local<Array> arrayNames = obj->GetOwnPropertyNames();

	int size = arrayNames->Length();
	if (size < 1)return false;
	
	sql += " (";
	for (int i = 0; i < size; ++i){
		Local<Value> key = arrayNames->Get(i);
		if (!Expression(obj->Get(key), sql, params)){
			return false;
		}
		if (i != size - 1){
			sql += " AND ";
		}
	}
	sql += ")";
	return true;

}

bool Sqlite3CipherDb::GreatThanExpression(Local<Value>& value, std::string& sql, std::vector<ParamNode*>& params)
{
	// Collect infos;
	Isolate* isolate = Isolate::GetCurrent();
	HandleScope scope(isolate);

	if (!value->IsArray())return false;

	Array* array = Array::Cast(*value);

	if (array->Length() != 2)return false;

	sql += " (";
	if (!Expression(array->Get(0), sql, params)){
		return false;
	}
	sql += " > ";
	if (!Expression(array->Get(1), sql, params)){
		return false;
	}
	sql += ") ";
	return true;
}

bool Sqlite3CipherDb::GreatThanEquExpression(Local<Value>& value, std::string& sql, std::vector<ParamNode*>& params)
{
	// Collect infos;
	Isolate* isolate = Isolate::GetCurrent();
	HandleScope scope(isolate);

	if (!value->IsArray())return false;

	Array* array = Array::Cast(*value);

	if (array->Length() != 2)return false;

	sql += " (";
	if (!Expression(array->Get(0), sql, params)){
		return false;
	}
	sql += " >= ";
	if (!Expression(array->Get(1), sql, params)){
		return false;
	}
	sql += ") ";
	return true;
}

bool Sqlite3CipherDb::LessThanExpression(Local<Value>& value, std::string& sql, std::vector<ParamNode*>& params)
{
	// Collect infos;
	Isolate* isolate = Isolate::GetCurrent();
	HandleScope scope(isolate);

	if (!value->IsArray())return false;

	Array* array = Array::Cast(*value);

	if (array->Length() != 2)return false;

	sql += " (";
	if (!Expression(array->Get(0), sql, params)){
		return false;
	}
	sql += " < ";
	if (!Expression(array->Get(1), sql, params)){
		return false;
	}
	sql += ") ";
	return true;
}

bool Sqlite3CipherDb::LessThanEquExpression(Local<Value>& value, std::string& sql, std::vector<ParamNode*>& params)
{
	// Collect infos;
	Isolate* isolate = Isolate::GetCurrent();
	HandleScope scope(isolate);

	if (!value->IsArray())return false;

	Array* array = Array::Cast(*value);

	if (array->Length() != 2)return false;

	sql += " (";
	if (!Expression(array->Get(0), sql, params)){
		return false;
	}
	sql += " <= ";
	if (!Expression(array->Get(1), sql, params)){
		return false;
	}
	sql += ") ";
	return true;
}

bool Sqlite3CipherDb::EquExpression(Local<Value>& value, std::string& sql, std::vector<ParamNode*>& params)
{
	// Collect infos;
	// Collect infos;
	Isolate* isolate = Isolate::GetCurrent();
	HandleScope scope(isolate);

	if (!value->IsObject())return false;

	Local<Object> obj = value->ToObject();
	Local<Array> arrayNames = obj->GetOwnPropertyNames();

	int size = arrayNames->Length();
	if (size < 1)return false;

	sql += " (";
	for (int i = 0; i < size; ++i){
		Local<Value> key = arrayNames->Get(i);
		Local<Value> value = obj->Get(key);

		if (!primitiveExpression(key, sql, params)){
			return false;
		}
		sql += " == ";
		
		if (!primitiveExpression(value, sql, params)){
			return false;
		}
		if (i != size - 1){
			sql += " AND ";
		}
	}
	sql += ")";
	return true;
}

bool Sqlite3CipherDb::ExistsExpression(Local<Value>& value, std::string& sql, std::vector<ParamNode*>& params)
{
	return false;
}

bool Sqlite3CipherDb::InExpression(Local<Value>& value, std::string& sql, std::vector<ParamNode*>& params)
{
	if (!value->IsObject()){
		return false;
	}
	Isolate* isolate = Isolate::GetCurrent();
	HandleScope scope(isolate);

	Local<Object> obj = value->ToObject();
	Local<Array> arrayNames = obj->GetOwnPropertyNames();
	int size = arrayNames->Length();

	if (size != 1)return false;

	Local<Value> key = arrayNames->Get(0);
	Local<Value> subvalue = obj->Get(key);

	if (!subvalue->IsArray())return false;

	sql += " (";
	if (!primitiveExpression(key, sql, params)){
		return false;
	}
	sql += " in (";
	Array* array = Array::Cast(*subvalue);
	int arraysize = array->Length();
	for (int i = 0; i < arraysize; ++i){
		if (!primitiveExpression(array->Get(i), sql, params)){
			return false;
		}
		if (i != arraysize - 1){
			sql += ",";
		}
	}
	sql += ")) ";
	return true;
}

bool Sqlite3CipherDb::NotInExpression(Local<Value>& value, std::string& sql, std::vector<ParamNode*>& params)
{
	if (!value->IsObject()){
		return false;
	}
	Isolate* isolate = Isolate::GetCurrent();
	HandleScope scope(isolate);

	Local<Object> obj = value->ToObject();
	Local<Array> arrayNames = obj->GetOwnPropertyNames();
	int size = arrayNames->Length();

	if (size != 1)return false;

	Local<Value> key = arrayNames->Get(0);
	Local<Value> subvalue = obj->Get(key);

	if (!subvalue->IsArray())return false;

	sql += " (";
	if (!primitiveExpression(key, sql, params)){
		return false;
	}
	sql += " not in (";
	Array* array = Array::Cast(*subvalue);
	int arraysize = array->Length();
	for (int i = 0; i < arraysize; ++i){
		if (!primitiveExpression(array->Get(i), sql, params)){
			return false;
		}
		if (i != arraysize - 1){
			sql += ",";
		}
	}
	sql += ")) ";
	return true;
}

bool Sqlite3CipherDb::primitiveExpression(Local<Value>& value, std::string& sql, std::vector<ParamNode*>& params)
{
	if (value->IsBoolean()){
		sql += "?";
		ParamNode* param = new ParamNode();
		param->type = PARAM_BOOL;
		param->boolVal = value->BooleanValue();
		params.push_back(param);
		return true;
	}
	else if (value->IsInt32() || value->IsUint32()){
		sql += "?";
		ParamNode* param = new ParamNode();
		param->type = PARAM_INT;
		param->integer = value->IsInt32() ? value->Int32Value() : value->Uint32Value();
		params.push_back(param);
		return true;
	}
	else if (value->IsDate()){ // sqlite不支持日期，无从判断因为将日期存为double
		sql += "?";
		ParamNode* param = new ParamNode();
		param->type = PARAM_DOUBLE;
		param->lfFloat = Date::Cast(*value)->ValueOf();
		params.push_back(param);
		return true;
	}
	else if (value->IsString()){
		sql += "?";
		ParamNode* param = new ParamNode();
		param->type = PARAM_TEXT;
		param->strText = (*String::Utf8Value(value->ToString()));
		params.push_back(param);
		return true;
	}
	else if (value->IsArrayBuffer()){
		sql += "?";
		ParamNode* param = new ParamNode();
		param->type = PARAM_BLOB;
		ArrayBuffer* pBuffer = ArrayBuffer::Cast(*value);
		param->blobbytes = pBuffer->ByteLength();
		param->pBlob = new unsigned char[param->blobbytes];
		params.push_back(param);
		return true;
	}
	else if (value->IsFloat32x4()){
		sql += "?";
		ParamNode* param = new ParamNode();
		param->type = PARAM_DOUBLE;
		param->lfFloat = value->NumberValue();
		params.push_back(param);
		return true;
	}
	return false;
}

}

