mark_as_advanced(LIBMYSQL_LIBRARY LIBMYSQL_INCLUDE_DIR)

find_path(LIBMYSQL_INCLUDE_DIR 
	NAMES mysql.h 
	PATH_SUFFIXES mysql
	PATHS
	~/Library/Frameworks
	/Library/Frameworks
	/usr/include
	/usr)

find_library(LIBMYSQL_LIBRARY 
	NAMES libmysql libmysqlclient.a
	PATHS
	~/Library/Frameworks
	/Library/Frameworks
	/usr/lib/x86_64-linux-gnu
	/usr/lib)
		
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(libmysql DEFAULT_MSG LIBMYSQL_LIBRARY LIBMYSQL_INCLUDE_DIR)

