vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO qpdf/qpdf
    REF v${VERSION}
    SHA512 9c57078b9fdfcbb1fb3330fac9b5f01279617a0d87c11a4cc01884e199f9936e49d9f7c9e88ac5fbb0ff660194017fc6218bb8929eba9ebd5b4ce56099a937bf
    PATCHES
        cmake-library-only.patch
)

string(COMPARE EQUAL "${VCPKG_LIBRARY_LINKAGE}" "static" BUILD_STATIC_LIBS)

vcpkg_check_features(OUT_FEATURE_OPTIONS FEATURE_OPTIONS
    FEATURES
        gnutls         REQUIRE_CRYPTO_GNUTLS
        openssl        REQUIRE_CRYPTO_OPENSSL
        zopfli         ZOPFLI
)

vcpkg_find_acquire_program(PKGCONFIG)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        ${FEATURE_OPTIONS}
        -DREQUIRE_CRYPTO_NATIVE=ON
        -DUSE_IMPLICIT_CRYPTO=OFF
        -DBUILD_STATIC_LIBS=${BUILD_STATIC_LIBS}
)

vcpkg_cmake_install()

vcpkg_cmake_config_fixup(CONFIG_PATH lib/cmake/qpdf)
vcpkg_copy_pdbs()
vcpkg_fixup_pkgconfig()

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/share")

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/NOTICE.md" "${SOURCE_PATH}/LICENSE.txt")
