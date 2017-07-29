var sqlite = require('../src/build/Release/sqlite3-cipher');

sqlite.connect('test.db','ffff',function (err, db){
    if (err){
        console.log(err);
        return;
    }
    db.exec("CREATE TABLE test ( first_field INTEGER, second_field INTEGER);INSERT INTO test VALUES(20,30)", function (err)
    {
        if (err){
            console.log(err);
            db.close();
            return;
        }
        db.exec("SELECT * FROM test",function (err, results){
            if (err){
                console.log(err);
                db.close();
                return;
            }
            console.log(results);
            db.close();
        })
    })
})