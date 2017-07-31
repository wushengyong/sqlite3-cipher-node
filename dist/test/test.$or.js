var sqlite = require('../sqlite3-cipher');
var config = require('./config');

sqlite.connect(config.dbPath,config.passwd, function (err, db){
    if (err){
        console.log(err);
    }
    else {
        db.table("test").find({$or : [{int64Field : 1E12},{int64Field : 10}]},function (err, items)
        {
            if (err){
                console.log(err);
            }
            else {
                if (items){
                    console.log("$or test OK !");
                }
            }
            
            db.close();
        },null);
    }
});