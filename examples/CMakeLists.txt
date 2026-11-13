add_executable(probnode_datasets datasets.cpp)
target_link_libraries(probnode_datasets PRIVATE probnode::probnode)

# Written in C++20 for the designated initializers of the options; the library itself is C++17.
add_executable(probnode_export_book export_book.cpp)
target_link_libraries(probnode_export_book PRIVATE probnode::probnode)
target_compile_features(probnode_export_book PRIVATE cxx_std_20)
