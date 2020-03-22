/* use either MHASH_ or RHASH_ for common hashing methods, libmhash is usually faster than librhash */
#define HASH_MHASH_CRC32
#define HASH_MHASH_SHA1
//#define HASH_MHASH_ADLER32
//#define HASH_RHASH_CRC32
//#define HASH_RHASH_SHA1
//#define HASH_OPENSSL_SHA1
//#define HASH_NETTLE_SHA1

//#define DO_REAL_COMPARE
//#define HASH_READ_BUFFER_SIZE 4096
#define HASH_READ_BUFFER_SIZE 65536
#define COMPARE_READ_BUFFER_SIZE 65536
//#define MINIMUM_DUPLICATE_FILE_SIZE 1024
#define MINIMUM_DUPLICATE_FILE_SIZE 8192
//force commit every 30 seconds or every 1000 records, but only check this every 10 records to be written
#define COMMIT_FORCE_CHECK_EVERY 10
#define COMMIT_FORCE_AMOUNT 1000
#define COMMIT_FORCE_INTERVAL 30
