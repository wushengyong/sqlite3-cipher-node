{
    'targets' : [
        {
            'target_name' : 'sqlite3-cipher',
            'defines': ["SQLITE_HAS_CODEC=1", "CODEC_TYPE=CODEC_TYPE_AES128", "SQLITE_CORE",
				"THREADSAFE","SQLITE_SECURE_DELETE","SQLITE_SOUNDEX","SQLITE_ENABLE_COLUMN_METADATA","_CRT_SECURE_NO_WARNINGS"], 
            'sources' : ['sqlite3ext.h','sqlite3.h','sqlite3.c','sqlite3cipherdb.h','sqlite3cipherdb.cpp','main.cpp']
        }
    ]
    
}