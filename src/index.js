var sqlite = require('./sqlite3-cipher');


const multi_params_name = ['$or', '$and'];
const multi_params_operator = [' OR ', ' AND '];

const two_params_name = ['$gt', '$lt', '$gte', '$lte'];
const two_params_operator = [' > ', ' < ', ' >= ', ' <= '];

const two_params_name_2 = ['$in', '$nin'];
const two_params_operator_2 = [' IN ', ' NOT IN '];

// - Attention : bind的只能是参数，不能是列名
function __parse_condition__(obj,binds)
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
                sql += __parse_condition__(obj[key][subkey],binds);
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
                total_sql += __parse_condition__(obj[key],binds);
            }
            else {
                total_sql += "(" + key +"=?)";
                binds.push(obj[key]);
            }
        }
    }
    return total_sql;
};

function table(tablename, db)
{
    this.tablename = tablename;
    this.db = db;
}

table.prototype.insert = function (obj,callback){
    if (Array.isArray(obj)){
        var self = this;
        (function iterate(err,index,items){
            if (err){
                callback(err);
            } else if (index < obj.length){
                self.insert(obj[index],function (err,items__){
                    iterate(err,index+1,items.concat(items__));
                })
            } else {
                callback(err,items);
            }
        })(null,0,[]);
        
    } else {
        var colnames = [];
        var values = [];
        var values_tag = [];
        for (var key in obj){
            if (typeof(obj[key]) == 'function')continue;
            colnames.push(key);
            values.push(obj[key]);
            values_tag.push('?');
        }
        if (!values){
            callback(null);
            return;
        }
        var sql = "INSERT INTO " + this.tablename + "(" + colnames.join(",") + ") VALUES (" + values_tag.join(",") + ")";
        this.db.prepare(sql, function (err, stmt){
            if (err){
                callback(err);
                return;
            }
            for (var index = 0 ; index < values.length ; ++ index){
                stmt.bind(index, values[index]);
            }
            stmt.exec(callback);
        });
    }
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
        var sql = __parse_condition__(findinfo.__conditions,binds);
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
table.prototype.update = function (obj, conditions, callback){
    var values = [];
    var update_sql = '';
    for (var key in obj){
        if (typeof(obj[key]) == 'function')continue;
        if (update_sql)update_sql += ",";
        update_sql += key + "= ?";
        values.push(obj[key]);
    }
    update_sql = "SET " + update_sql;
    if (!values){
        callback(null);
        return;
    }
    var condition_sql = __parse_condition__(conditions,values);
    var sql = "UPDATE " + this.tablename + " " + update_sql ;
    if (condition_sql){
        sql += " WHERE " + condition_sql;
    }
    
    this.db.prepare(sql, function (err, stmt){
        if (err){
            callback(err);
            return;
        }
        for (var index = 0 ; index < values.length ; ++ index){
            stmt.bind(index, values[index]);
        }
        stmt.exec(callback);
    });
}

table.prototype.delete = function (conditions, callback){
    var values = [];
    var condition_sql = __parse_condition__(conditions,values);
    var sql = "DELETE FROM " + this.tablename;
    if (condition_sql){
        sql += " WHERE " + condition_sql;
    }
    
    this.db.prepare(sql, function (err, stmt){
        if (err){
            callback(err);
            return;
        }
        for (var index = 0 ; index < values.length ; ++ index){
            stmt.bind(index, values[index]);
        }
        stmt.exec(callback);
    });
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