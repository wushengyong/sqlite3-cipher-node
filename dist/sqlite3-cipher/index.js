var sqlite = require('./sqlite3-cipher');


function table(tablename, db)
{
    this.tablename = tablename;
    this.db = db;
}

table.prototype.insert = function (){

};

const multi_params_name = ['$or', '$and'];
const multi_params_operator = [' OR ', ' AND '];

const two_params_name = ['$gt', '$lt', '$gte', '$lte'];
const two_params_operator = [' > ', ' < ', ' >= ', ' <= '];

const two_params_name_2 = ['$in', '$nin'];
const two_params_operator_2 = [' IN ', ' NOT IN '];

// - Attention : bind的只能是参数，不能是列名
function __parse(obj,binds)
{
    if (obj == null)return "";
    var total_sql = "";
    for (var key in obj){
        if (typeof (obj[key]) == "function")continue;
        if (total_sql)total_sql += " AND ";
        if (multi_params_name.indexOf(key) != -1){
            var sql = "";
            var operator = multi_params_operator[multi_params_name.indexOf(key)];
            for (var subkey in obj[key]){
                if (typeof (obj[key][subkey]) == "function")continue;
                if (sql)sql += operator;
                sql += __parse(obj[key][subkey],binds);
            }
            total_sql += "(" + sql + ")";
        }
        else if (two_params_name.indexOf(key) != -1){
            var sql = "";
            var operator = two_params_operator[two_params_name.indexOf(key)];
            sql += "(" + obj[key][0] + operator + "?)";
            binds.push(obj[key][1]);
            total_sql += "(" + sql + ")";            
        }
        else if (two_params_name_2.indexOf(key) != -1){
            var sql = "";
            var operator = two_params_operator_2[two_params_name_2.indexOf(key)];
            //sql += '(?' + operator + "(";
            for (var subkey in obj[key][1]){
                if (typeof(obj[key][1][subkey]) == "function")continue;
                if (sql)sql += ",";
                sql += "?";
                binds.push(obj[key][1][subkey]);
            }
            sql = obj[key][0] + operator + "(" + sql + ")";
            total_sql += "(" + sql + ")";            
        }
        else {
            // condidtions[key]是object，同时不是buff，则让其继续解析，否则直接赋值相等
            if (typeof (obj[key]) == "object" && !Buffer.isBuffer(obj[key])){
                total_sql += __parse(obj[key],binds);
            }
            else {
                total_sql += "(" + key +"=?)";
                binds.push(obj[key]);
            }
        }
    }
    return total_sql;
};

table.prototype.find = function (conditions,callback,fields) {
    var findinfo = {
        __tablename : this.tablename,
        __db : this.db,
        __conditions : conditions,
        __fields : fields,
        __callback : callback,
        count : function (){
            this.__count = true;
            return this;
        },
        limit : function (limit){
            this.__limit = limit;
            return this;
        },
        skip : function (skip){
            this.__skip = skip;
            return this;
        },
        sort : function (sort){
            this.__sort = sort;
            return this;
        }
    };
    process.nextTick(function (){
        var binds = [];
        var sql = __parse(findinfo.__conditions,binds);
        var total_sql = "";
        if (findinfo.__count){
            total_sql = "SELECT COUNT(*) FROM " + findinfo.__tablename;
        } else {
            if (findinfo.__fields){
                total_sql = "SELECT " + findinfo.__fields.join(",") + " FROM " + findinfo.__tablename;
            }
            else {
                total_sql = "SELECT * FROM " + findinfo.__tablename;
            }
        }
        if (sql){
            total_sql += " WHERE " + sql;
        }
        if (findinfo.__sort){
            for (var key in findinfo.__sort){
                if (typeof(findinfo.__sort[key]) == "function")continue;
                if (findinfo.__sort[key] > 0){
                    total_sql += " ORDER BY " + key + "ASC ";
                } else {
                    total_sql += " ORDER BY " + key + "DESC";
                }
            }
        }
        if (findinfo.__limit){
            total_sql += " LIMIT " + findinfo.__limit;
        }
        if (findinfo.__skip){
            total_sql += " OFFSET " + findinfo.__skip;
        }
        findinfo.__db.prepare(total_sql,function (err, stmt){
            if (err){
                findinfo.__callback(err);
                return;
            }
            for (var i = 0 ; i < binds.length; ++i){
                if (!stmt.bind(i,binds[i])){
                    findinfo.__callback(Error("Cannot bind params"));
                }
            }
            stmt.exec(findinfo.__callback);
        });
    });
    return findinfo;
}
table.prototype.update = function (){

}

table.prototype.delete = function (){
    
}

exports.connect = function (dbpath,key,callback)
{
    sqlite.connect(dbpath,key,function(err, db){
        db.drop = function (tablename){
            console.log(this);
        };
        db.table = function (tablename){
            return new table(tablename,this);
        }
        callback(err,db);
    })
};
//  filter : $or 
//  filter : $and
//  filter : $gt
//  filter : $lt
//  filter : $gte
//  filter : $lte
//  filter : $in
//  filter : $nin


// expr : {$or : [expr, expr ...]} | {$and : [expr,expr ...]} | 
//         {$gt : [string, primivte]} | {$lt : [string, primitive]} | {$gte : [string, primitive]} | {$lte : [string, primitive]} |
//       | {$in : [a,b,c,d]} | {$nin : [a,b,c,d]} | {string:string} | 就是and[]
//         

// 如何实现，思路：
// 1.  一般都是挂在find后面，可以在find中特殊处理，中间加一层js object，延迟执行find比如Process.nextTick
// 2.  .count .skip .limit均可以设置js object的成员变量
// 3.
// .count() 数量
// .skip()
// .limit()
// .sort({KEY : 1}) .sort({KEY : -1})
/*sqlite.connect(function (err, db){
    db.insert(data,function (err, items){});
    db.find(filter,function (err, items){});
    db.update(filter,data,function (err,items){});
    db.delete(filter,function (err, items){})
});*/