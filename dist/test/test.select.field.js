var sqlite = require('../sqlite3-cipher');
var config = require('./config');

sqlite.connect(config.dbPath,config.passwd, function (err, db){
    if (err){
        console.log(err);
    }
    else {
        db.table("test").find({$in : ['intField',[1,2,5,10]]},function (err, items)
        {
            if (err){
                console.log(err);
            }
            else {
                if (items){
                    console.log("select special fields test OK !");
                }
            }
            
            db.close();
        },['intField','int64Field']);
    }
});