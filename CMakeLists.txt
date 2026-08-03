cmake_minimum_required(VERSION 3.20)

project(knord VERSION 0.1.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
include(GNUInstallDirs)

set(QT_MIN_VERSION 6.5)
find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Widgets)

# Found per-component rather than through the KF6 umbrella package, which is
# provided by extra-cmake-modules. Keeping ECM out of the build means the only
# build deps are the KF6 runtime libraries that are already pulled in by Plasma.
find_package(KF6I18n REQUIRED)
find_package(KF6CoreAddons REQUIRED)
find_package(KF6Config REQUIRED)
find_package(KF6Notifications REQUIRED)
find_package(KF6IconThemes REQUIRED)
find_package(KF6StatusNotifierItem REQUIRED)
find_package(KF6WindowSystem REQUIRED)

qt_standard_project_setup(REQUIRES 6.5)

qt_add_executable(knord
    src/main.cpp
    src/clirunner.h
    src/clirunner.cpp
    src/ptychannel.h
    src/ptychannel.cpp
    src/nordvpn.h
    src/nordvpn.cpp
    src/nordparse.h
    src/nordparse.cpp
    src/locationmodel.h
    src/locationmodel.cpp
    src/countryflags.h
    src/countryflags.cpp
    src/trayicon.h
    src/trayicon.cpp
    src/appsettings.h
    src/appsettings.cpp
)

qt_add_qml_module(knord
    URI io.github.timpalpant.knord
    VERSION 1.0
    QML_FILES
        src/qml/Main.qml
        src/qml/StatusPage.qml
        src/qml/LocationsPage.qml
        src/qml/CitiesPage.qml
        src/qml/SettingsPage.qml
        src/qml/AccountPage.qml
        src/qml/LoginPage.qml
        src/qml/AllowlistPage.qml
        src/qml/LocationDelegate.qml
        src/qml/DetailRow.qml
)

# The generated QML type registrations include the declaring headers by bare
# name, so src/ has to be reachable as an include directory.
target_include_directories(knord PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(knord PRIVATE
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Widgets
    KF6::I18n
    KF6::I18nQml
    KF6::CoreAddons
    KF6::ConfigCore
    KF6::Notifications
    KF6::IconThemes
    KF6::StatusNotifierItem
    KF6::WindowSystem
)

include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

install(TARGETS knord RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES data/io.github.timpalpant.knord.desktop
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications)
install(FILES data/io.github.timpalpant.knord.svg
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/scalable/apps)
install(FILES data/io.github.timpalpant.knord.metainfo.xml
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/metainfo)
# Keeps the KAboutData component name, which is what KNotification looks up.
install(FILES data/knord.notifyrc
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/knotifications6)

# The CI and release workflows build native DEB and RPM packages from the
# normal install tree, so keep their metadata next to the install rules.
set(CPACK_PACKAGE_NAME "knord")
set(CPACK_PACKAGE_VENDOR "Timothy Palpant")
set(CPACK_PACKAGE_CONTACT "Timothy Palpant <tim@palpant.us>")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Native NordVPN client for KDE Plasma")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "${CPACK_PACKAGE_CONTACT}")
set(CPACK_DEBIAN_PACKAGE_SECTION "net")
set(CPACK_DEBIAN_PACKAGE_DEPENDS
    "nordvpn, qml6-module-qtquick, qml6-module-qtquick-controls, qml6-module-qtquick-layouts, qml6-module-org-kde-kirigami, qml6-module-org-kde-kirigamiaddons-formcard")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_RPM_PACKAGE_LICENSE "GPL-3.0-or-later")
set(CPACK_RPM_PACKAGE_GROUP "Applications/Internet")
set(CPACK_RPM_PACKAGE_REQUIRES "nordvpn, qt6-qtdeclarative, kf6-kirigami, kf6-kirigami-addons")
set(CPACK_RPM_FILE_NAME RPM-DEFAULT)
include(CPack)
