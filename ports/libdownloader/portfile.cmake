set(SOURCE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../libdownloader")

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
)

vcpkg_cmake_install()
vcpkg_copy_pdbs()