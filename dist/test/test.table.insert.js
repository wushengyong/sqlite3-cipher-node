var sqlite = require('../sqlite3-cipher');
var config = require('./config');

sqlite.connect(config.dbPath,config.passwd, function (err, db){
    if (err){
        console.log(err);
    }
    else {
        db.table('test').insert([{intField : 1, int64Field : 2000},{int64Field:1000}],function (err, items){
            if (err){
                console.log(err);
            } else {
                console.log("table.test.insert OK !");
            }
            db.close();
        });
    }
});