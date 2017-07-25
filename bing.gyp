{
    'targets' : [
        {
            'target_name' : 'sqlite3-cipher',
            'defines': ['SQLITE_HAS_CODEC','CODEC_TYPE=CODEC_TYPE_AES128','SQLITE_CORE','SQLITE_SECURE_DELETE','SQLITE_ENABLE_COLUMN_METADATA','SQLITE_ENABLE_RTREE'],
            'sources' : ['sqlite3tex.h','sqlite3.h','sqlite3.c']
        }
    ]
    
}