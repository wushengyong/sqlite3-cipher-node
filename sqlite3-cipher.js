var sqlite = require('../build/Release/sqlite3-cipher');

//  filter : $or 
//  filter : $and
//  filter : $gt
//  filter : $lt
//  filter : $gte
//  filter : $lte
//  filter : $all
//  filter : $exists
//  filter : $in
//  filter : $nin

// 如何实现，思路：
// 1.  一般都是挂在find后面，可以在find中特殊处理，中间加一层js object，延迟执行find比如Process.nextTick
// 2.  .count .skip .limit均可以设置js object的成员变量
// 3.
// .count() 数量
// .skip()
// .limit()
// .sort({KEY : 1}) .sort({KEY : -1})

// 我的node插件要开放什么接口
// connect(path, key, callback) --> callback 给出db
// db具有exec跟close
// exec(JSON, callback)
// JSON要包含数据 maxCnt, skipCnt, limit, sortKey, operateType -- INSERT , FIND, UPDATE, DELETE,   insertObject, queryObject, updateFilter, updateObject, deleteObject
// close()


// 开放一条接口 connect 
// 增加一个ObjectWrapper   db
sqlite.connect(function (err, db){
    db.insert(data,function (err, items){});
    db.find(filter,function (err, items){});
    db.update(filter,data,function (err,items){});
    db.delete(filter,function (err, items){})
});