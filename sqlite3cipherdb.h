#ifndef SQLITE3_CIPHER_DB_H_
#define SQLITE3_CIPHER_DB_H_

#include <node.h>
#include <node_object_wrap.h>
#include "sqlite3.h"
#include <uv.h>
#include <vector>

namespace sqlite3_cipher{

using namespace v8;

enum PARAM_TYPE
{
	PARAM_TEXT,
	PARAM_INT,
	PARAM_DOUBLE,
	PARAM_BLOB,
	PARAM_BOOL,
};

class ParamNode
{
public:
	ParamNode() : pBlob(NULL), blobbytes(0){}
	~ParamNode() {
		if (pBlob != NULL){
			delete[] pBlob;
		}
	}

	PARAM_TYPE type;
	std::string strText;
	__int64 integer;
	double lfFloat;
	bool boolVal;
	unsigned char* pBlob;
	__int64 blobbytes;
};
class Sqlite3CipherDb : public node::ObjectWrap
{
public:
    static void Init();
    static void NewInstance(const FunctionCallbackInfo<Value>& args);

private:
    explicit Sqlite3CipherDb();
    ~Sqlite3CipherDb();

    static void New(const FunctionCallbackInfo<Value>& args);
	/************************************************************************/
	/* exec                                                                 */
	/************************************************************************/
	// JSON       sql object
	// callback   if is none , don't callback
    static void exec(const FunctionCallbackInfo<Value>& args);
	/************************************************************************/
	/* close disconnect db                                                  */
	/************************************************************************/
	// callback   if is none, don't callback
    static void close(const FunctionCallbackInfo<Value>& args);


	static void exec_ing_(uv_work_t* req);
	static void exec_done_(uv_work_t* req, int status);

    static Persistent<Function> constructor;

	static bool Expression(Local<Value>& value, std::string& sql, std::vector<ParamNode*>& params);
	static bool OrExpression(Local<Value>& value, std::string& sql, std::vector<ParamNode*>& params);
	static bool AndExpression(Local<Value>& value, std::string& sql, std::vector<ParamNode*>& params);
	static bool GreatThanExpression(Local<Value>& value, std::string& sql, std::vector<ParamNode*>& params);
	static bool GreatThanEquExpression(Local<Value>& value, std::string& sql, std::vector<ParamNode*>& params);
	static bool LessThanExpression(Local<Value>& value, std::string& sql, std::vector<ParamNode*>& params);
	static bool LessThanEquExpression(Local<Value>& value, std::string& sql, std::vector<ParamNode*>& params);
	static bool EquExpression(Local<Value>& value, std::string& sql, std::vector<ParamNode*>& params);
	static bool ExistsExpression(Local<Value>& value, std::string& sql, std::vector<ParamNode*>& params);
	static bool InExpression(Local<Value>& value, std::string& sql, std::vector<ParamNode*>& params);
	static bool NotInExpression(Local<Value>& value, std::string& sql, std::vector<ParamNode*>& params);

	static bool primitiveExpression(Local<Value>& value, std::string& sql, std::vector<ParamNode*>& params);
    sqlite3_stmt* m_pDB; 
};

}
#endif