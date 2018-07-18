add_library(log_lib log.cpp)

add_library(conf_lib conf.cpp)
target_link_libraries(conf_lib ${Boost_LIBRARIES})
target_link_libraries(conf_lib log_lib)

add_library(test_common_lib test_common.cpp)
target_link_libraries(test_common_lib ${Boost_LIBRARIES})

add_library(test_main test_main.cpp)
target_link_libraries(test_main gtest)
target_link_libraries(test_main conf_lib)

add_library(exceptions_lib exceptions.cpp)
target_link_libraries(exceptions_lib ${Boost_LIBRARIES})

add_library(file_tree_lib file_tree.cpp)
target_link_libraries(file_tree_lib ${Boost_LIBRARIES})
target_link_libraries(file_tree_lib conf_lib)

add_library(synch_thread_pool_lib synch_thread_pool.cpp)
target_link_libraries(file_tree_lib ${Boost_LIBRARIES})

add_executable(file_tree_test file_tree_test.cpp)
target_link_libraries(file_tree_test file_tree_lib)
target_link_libraries(file_tree_test test_main)
target_link_libraries(file_tree_test test_common_lib)
add_test(file_tree_test file_tree_test)

add_library(db_lib db_lib.cpp)
target_link_libraries(db_lib ${Boost_LIBRARIES})
target_link_libraries(db_lib ${Sqlite3_LIBRARIES})
target_link_libraries(db_lib log_lib)
target_link_libraries(db_lib exceptions_lib)

add_executable(db_lib_test db_lib_test.cpp)
target_link_libraries(db_lib_test db_lib)
target_link_libraries(db_lib_test test_main)
add_test(db_lib_test db_lib_test)

add_library(hash_cache_lib hash_cache.cpp)
target_link_libraries(hash_cache_lib ${Boost_LIBRARIES})
target_link_libraries(hash_cache_lib ${OPENSSL_CRYPTO_LIBRARY})
target_link_libraries(hash_cache_lib exceptions_lib)
target_link_libraries(hash_cache_lib log_lib)
target_link_libraries(hash_cache_lib db_lib)

add_library(scanner_lib scanner.cpp)
target_link_libraries(scanner_lib ${Boost_LIBRARIES})

add_executable(scanner_test scanner_test.cpp)
target_link_libraries(scanner_test scanner_lib)
target_link_libraries(scanner_test test_common_lib)
target_link_libraries(scanner_test test_main)
add_test(scanner_test scanner_test)

add_library(fuzzy_dedup_lib fuzzy_dedup.cpp)
target_link_libraries(fuzzy_dedup_lib ${Boost_LIBRARIES})
target_link_libraries(fuzzy_dedup_lib hash_cache_lib)
target_link_libraries(fuzzy_dedup_lib file_tree_lib)
target_link_libraries(fuzzy_dedup_lib synch_thread_pool_lib)

add_executable(fuzzy_dedup_test fuzzy_dedup_test.cpp)
target_link_libraries(fuzzy_dedup_test fuzzy_dedup_lib)
target_link_libraries(fuzzy_dedup_test test_main)
target_link_libraries(fuzzy_dedup_test scanner_lib)
add_test(fuzzy_dedup_test fuzzy_dedup_test)

add_library(db_output_lib db_output.cpp)
target_link_libraries(db_output_lib ${Boost_LIBRARIES})
target_link_libraries(db_output_lib db_lib)
target_link_libraries(db_output_lib fuzzy_dedup_lib)

add_library(dir_compare_lib dir_compare.cpp)
target_link_libraries(db_output_lib ${Boost_LIBRARIES})
target_link_libraries(db_output_lib conf_lib)
target_link_libraries(db_output_lib hash_cache_lib)
target_link_libraries(db_output_lib scanner_lib)
target_link_libraries(db_output_lib synch_thread_pool_lib)

add_executable(dupa dupa.cpp)
target_link_libraries(dupa ${Boost_LIBRARIES})
target_link_libraries(dupa file_tree_lib)
target_link_libraries(dupa hash_cache_lib)
target_link_libraries(dupa exceptions_lib)
target_link_libraries(dupa fuzzy_dedup_lib)
target_link_libraries(dupa log_lib)
target_link_libraries(dupa db_output_lib)
target_link_libraries(dupa dir_compare_lib)

install(TARGETS dupa RUNTIME DESTINATION bin)
