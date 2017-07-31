var sqlite = require('../sqlite3-cipher');
var config = require('./config');

sqlite.connect(config.dbPath, config.passwd,function (err, db){
    if (err){
        console.log(err);
    }else{
        db.prepare('INSERT INTO test VALUES(?,?,?,?,?)',function (err, stmt){
            console.log("bind int: ",stmt.bind(0,10));
            console.log("bind large int: ",stmt.bind(1,1E12));
            console.log("bind large double: ",stmt.bind(2,0.3));
            console.log("bind large string: ",stmt.bind(3,'I just test.'));
            console.log("bind large buff: ",stmt.bind(4, new Buffer([0x1,0x2,0x3,0x4])));
            stmt.exec(function (err){
                if (err){
                    console.log(err);
                }
                db.close();
            });
        });
    };
});