find_package(doctest CONFIG REQUIRED)

# One test executable per module, so every module is tested separately.
add_subdirectory(common)
add_subdirectory(downloader)
