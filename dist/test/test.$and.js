var sqlite = require('../sqlite3-cipher');
var config = require('./config');

sqlite.connect(config.dbPath,config.passwd, function (err, db){
    if (err){
        console.log(err);
    }
    else {
        db.table("test").find({$and : [{int64Field : 1E12},{intField : 10}]},function (err, items)
        {
            if (err){
                console.log(err);
            }
            else {
                if (items){
                    console.log("$and test OK !");
                }
            }
            
            db.close();
        },null);
    }
});