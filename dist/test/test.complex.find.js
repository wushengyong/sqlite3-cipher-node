var sqlite = require('../sqlite3-cipher');
var config = require('./config');

sqlite.connect(config.dbPath,config.passwd, function (err, db){
    if (err){
        console.log(err);
    }
    else {
        db.table("test").find([{$in : ['intField',[1,2,5,10]]},{$or : [{$gt : ['int64Field',1E10]},{$lt : ['intField',20]}]}],function (err, items)
        {
            if (err){
                console.log(err);
            }
            else {
                if (items){
                    console.log("$complex find test OK !");
                }
            }
            
            db.close();
        },null);
    }
});