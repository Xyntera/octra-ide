cmake_minimum_required(VERSION 3.21)

project(octra_wallet_native LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

option(OCTRA_BUILD_QT_APP "Build the Qt desktop app" ON)

if(OPENSSL_INCLUDE_DIR AND OPENSSL_CRYPTO_LIBRARY AND OPENSSL_SSL_LIBRARY)
    add_library(OpenSSL::Crypto UNKNOWN IMPORTED)
    set_target_properties(OpenSSL::Crypto PROPERTIES
        IMPORTED_LOCATION "${OPENSSL_CRYPTO_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${OPENSSL_INCLUDE_DIR}"
    )

    add_library(OpenSSL::SSL UNKNOWN IMPORTED)
    set_target_properties(OpenSSL::SSL PROPERTIES
        IMPORTED_LOCATION "${OPENSSL_SSL_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${OPENSSL_INCLUDE_DIR}"
        INTERFACE_LINK_LIBRARIES OpenSSL::Crypto
    )
else()
    find_package(OpenSSL REQUIRED)
endif()

if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64|AMD64)$")
    add_compile_options(-maes)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)$")
    add_compile_options(-march=armv8-a+crypto)
endif()

add_library(octra_pvac STATIC
    pvac/pvac_c_api.cpp
)
target_include_directories(octra_pvac PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/pvac
    ${CMAKE_CURRENT_SOURCE_DIR}/pvac/include
)

add_library(octra_crypto STATIC
    lib/tweetnacl.c
    lib/randombytes.c
)
target_include_directories(octra_crypto PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/lib
)

add_library(octra_core STATIC
    native/octra_core.cpp
)
target_include_directories(octra_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/lib
    ${CMAKE_CURRENT_SOURCE_DIR}/native
    ${CMAKE_CURRENT_SOURCE_DIR}/pvac
    ${CMAKE_CURRENT_SOURCE_DIR}/pvac/include
)
target_compile_definitions(octra_core PUBLIC CPPHTTPLIB_OPENSSL_SUPPORT)
target_link_libraries(octra_core PUBLIC
    octra_crypto
    octra_pvac
    OpenSSL::SSL
    OpenSSL::Crypto
)

if(WIN32)
    target_link_libraries(octra_core PUBLIC
        ws2_32
        crypt32
        bcrypt
    )
endif()

if(OCTRA_BUILD_QT_APP)
    find_package(Qt6 COMPONENTS Core Gui Qml Quick QuickControls2 QUIET)
    if(Qt6_FOUND)
        qt_add_executable(octra_wallet_native
            native/qt_main.cpp
            native/qt_app_bridge.cpp
        )
        set_target_properties(octra_wallet_native PROPERTIES WIN32_EXECUTABLE TRUE)

        qt_add_qml_module(octra_wallet_native
            URI OctraWallet
            VERSION 1.0
            QML_FILES
                native/qml/Main.qml
        )

        qt_add_resources(octra_wallet_native "octra_wallet_templates"
            PREFIX "/octra_templates"
            FILES
                static/templates/index.json
                static/templates/empty/main.aml
                static/templates/token/main.aml
                static/templates/token/interfaces/IOCS01.aml
                static/templates/vault/main.aml
                static/templates/amm/main.aml
                static/templates/escrow/main.aml
                static/templates/multisig/main.aml
        )

        target_link_libraries(octra_wallet_native PRIVATE
            Qt6::Core
            Qt6::Gui
            Qt6::Qml
            Qt6::Quick
            Qt6::QuickControls2
            octra_core
        )
    else()
        message(WARNING "Qt6 not found. Building octra_core only.")
    endif()
endif()
