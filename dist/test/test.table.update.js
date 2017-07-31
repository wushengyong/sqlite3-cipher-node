var sqlite = require('../sqlite3-cipher');
var config = require('./config');

sqlite.connect(config.dbPath,config.passwd, function (err, db){
    if (err){
        console.log(err);
    }
    else {
        db.table('test').update({doubleField : 4.0, intField : 20},{intField : 1, int64Field : 2000},function (err, items){
            if (err){
                console.log(err);
            } else {
                console.log("table.test.update OK !");
            }
            db.close();
        });
    }
});