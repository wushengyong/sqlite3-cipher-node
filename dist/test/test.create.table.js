var sqlite = require('../sqlite3-cipher');
var config = require('./config');

sqlite.connect(config.dbPath, config.passwd,function (err, db){
    if (err){
        console.log(err);
    }else{
        db.exec("CREATE TABLE test ( intField INTEGER, int64Field INTEGER, doubleField DOUBLE, stringField TEXT, blobField BLOB);", function (err)
        {
            if (err){
                console.log(err);
            }
            else {
                console.log("create table test.");
            }
            db.close();
        });
    };
    console.log("Create db : " + config.dbPath + " with key : " + config.passwd);
})