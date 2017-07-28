var sqlite = require('./build/Release/sqlite3-cipher');

sqlite.connect('test.db','abcdefg',function (err, db){
    if (err){
        console.log(err);
        return;
    }
    console.log(db);
    db.close(function (err){
		if (err){
			console.log(err);
			return;
		}
		console.log("closed.");
	});
})