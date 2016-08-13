add_library(log_lib log.cpp)

add_library(conf_lib conf.cpp)
target_link_libraries(conf_lib ${Boost_LIBRARIES})
target_link_libraries(conf_lib log_lib)

add_library(test_main test_main.cpp)
target_link_libraries(test_main gtest)

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
add_test(file_tree_test file_tree_test)

add_library(sql_lib sql_lib.cpp)
target_link_libraries(sql_lib ${Boost_LIBRARIES})
target_link_libraries(sql_lib ${Sqlite3_LIBRARIES})

add_library(hash_cache_lib hash_cache.cpp)
target_link_libraries(hash_cache_lib ${Boost_LIBRARIES})
target_link_libraries(hash_cache_lib ${PROTOBUF_LIBRARIES})
target_link_libraries(hash_cache_lib ${OPENSSL_CRYPTO_LIBRARY})
target_link_libraries(hash_cache_lib exceptions_lib)
target_link_libraries(hash_cache_lib log_lib)
target_link_libraries(hash_cache_lib sql_lib)

add_library(fuzzy_dedup_lib fuzzy_dedup.cpp)
target_link_libraries(fuzzy_dedup_lib ${Boost_LIBRARIES})
target_link_libraries(fuzzy_dedup_lib hash_cache_lib)
target_link_libraries(fuzzy_dedup_lib file_tree_lib)
target_link_libraries(fuzzy_dedup_lib synch_thread_pool_lib)

add_executable(fuzzy_dedup_test fuzzy_dedup_test.cpp)
target_link_libraries(fuzzy_dedup_test fuzzy_dedup_lib)
target_link_libraries(fuzzy_dedup_test test_main)
add_test(fuzzy_dedup_test fuzzy_dedup_test)

add_library(db_output_lib db_output.cpp)
target_link_libraries(db_output_lib ${Boost_LIBRARIES})
target_link_libraries(db_output_lib sql_lib)
target_link_libraries(db_output_lib fuzzy_dedup_lib)

add_executable(dup_ident dup_ident.cpp)
target_link_libraries(dup_ident ${Boost_LIBRARIES})
target_link_libraries(dup_ident file_tree_lib)
target_link_libraries(dup_ident hash_cache_lib)
target_link_libraries(dup_ident exceptions_lib)
target_link_libraries(dup_ident fuzzy_dedup_lib)
target_link_libraries(dup_ident synch_thread_pool_lib)
target_link_libraries(dup_ident log_lib)
target_link_libraries(dup_ident db_output_lib)
target_link_libraries(dup_ident conf_lib)

install(TARGETS dup_ident RUNTIME DESTINATION bin)
