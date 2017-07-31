var sqlite = require('../sqlite3-cipher');
var config = require('./config');

sqlite.connect(config.dbPath,config.passwd, function (err, db){
    if (err){
        console.log(err);
    }
    else {
        db.table("test").find({$gt : ['intField',5]},function (err, items)
        {
            if (err){
                console.log(err);
            }
            else {
                if (items){
                    console.log("$gt test OK !");
                }
            }
            
            db.close();
        },null);
    }
});