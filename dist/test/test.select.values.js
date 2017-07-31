var sqlite = require('../sqlite3-cipher');
var config = require('./config');

sqlite.connect(config.dbPath, config.passwd,function (err, db){
    if (err){
        console.log(err);
    }else{
        db.exec('SELECT * FROM test;', function (err, items){
            if (err){
                console.log(err);
            } else {
                console.log(items);
                db.close();
            }
        });
    };
});