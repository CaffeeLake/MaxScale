add_executable(test_sqlite_strlike test_sqlite_strlike.cc ../sqlite_strlike.cc)
target_link_libraries(test_sqlite_strlike maxbase)
add_test(test_sqlite_strlike test_sqlite_strlike)
