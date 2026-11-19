cmake_minimum_required(VERSION 3.21)

project(clipboard-linux
    VERSION 0.1.0
    DESCRIPTION "Clipboard history manager for Linux, inspired by Windows Win+V"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_AUTOMOC ON)

option(BUILD_TESTING "Build the unit tests" ON)

find_package(Qt6 6.4 REQUIRED COMPONENTS Widgets Sql DBus)
find_package(PkgConfig REQUIRED)
pkg_check_modules(XCB REQUIRED IMPORTED_TARGET xcb)

add_executable(clipboard-linux
    include/ClipboardWatcher.h
    include/Daemon.h
    include/HistoryModel.h
    include/HistoryStore.h
    include/ItemDelegate.h
    include/Paster.h
    include/PopupWindow.h
    include/Theme.h
    include/X11Activation.h
    src/ClipboardWatcher.cpp
    src/Daemon.cpp
    src/HistoryModel.cpp
    src/HistoryStore.cpp
    src/ItemDelegate.cpp
    src/Paster.cpp
    src/PopupWindow.cpp
    src/X11Activation.cpp
    src/main.cpp
)

target_include_directories(clipboard-linux PRIVATE include)

target_compile_definitions(clipboard-linux PRIVATE
    APP_VERSION="${PROJECT_VERSION}")

target_compile_options(clipboard-linux PRIVATE -Wall -Wextra -Wpedantic)

target_link_libraries(clipboard-linux PRIVATE Qt6::Widgets Qt6::Sql Qt6::DBus PkgConfig::XCB)

if(BUILD_TESTING)
    enable_testing()
    find_package(Qt6 REQUIRED COMPONENTS Test)

    add_executable(test_history_store
        tests/test_history_store.cpp
        include/HistoryStore.h
        src/HistoryStore.cpp
    )
    target_include_directories(test_history_store PRIVATE include)
    target_link_libraries(test_history_store PRIVATE Qt6::Sql Qt6::Test)
    target_compile_options(test_history_store PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME test_history_store COMMAND test_history_store)
endif()

include(GNUInstallDirs)
install(TARGETS clipboard-linux RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
# The app runs this on first start to add the GNOME shortcuts.
install(PROGRAMS packaging/gnome-shortcuts.py
    DESTINATION ${CMAKE_INSTALL_BINDIR}
    RENAME clipboard-linux-shortcuts)

# System-wide files for a .deb package (packaging/build-deb.sh turns this on).
# install.sh installs per user instead, so it leaves this off.
option(INSTALL_SYSTEM_FILES "Install udev rule, uinput autoload and system-wide autostart" OFF)
if(INSTALL_SYSTEM_FILES)
    set(BINARY ${CMAKE_INSTALL_FULL_BINDIR}/clipboard-linux)
    configure_file(packaging/clipboard-linux.desktop clipboard-linux.desktop @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/clipboard-linux.desktop
        DESTINATION ${CMAKE_INSTALL_FULL_SYSCONFDIR}/xdg/autostart)
    install(FILES packaging/60-clipboard-linux-uinput.rules
        DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/udev/rules.d)
    install(FILES packaging/modules-load.conf
        DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/modules-load.d
        RENAME clipboard-linux.conf)
endif()

set(CPACK_PACKAGE_NAME clipboard-linux)
set(CPACK_PACKAGE_VENDOR "AfsarTanvir")
set(CPACK_PACKAGE_CONTACT "AfsarTanvir <afsartanvir805@gmail.com>")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Clipboard history for GNOME (Super+V, like Windows)")
set(CPACK_PACKAGE_DESCRIPTION
    "Keeps your clipboard history across reboots. Super+V opens the list, Enter or a click
pastes, Ctrl+Alt+1..9 paste items directly. Items can be pinned, removed or cleared.")
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/AfsarTanvir/ClipBoard-for-Linux")
set(CPACK_DEBIAN_PACKAGE_SECTION utils)
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
# Library dependencies are found automatically; these are needed at run time
# but not linked: the SQLite plugin, gdbus for the shortcuts, python3 for the
# shortcut script.
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libqt6sql6-sqlite, libglib2.0-bin, python3")
set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA
    ${CMAKE_CURRENT_SOURCE_DIR}/packaging/deb/postinst
    ${CMAKE_CURRENT_SOURCE_DIR}/packaging/deb/prerm
    ${CMAKE_CURRENT_SOURCE_DIR}/packaging/deb/postrm)
set(CPACK_DEBIAN_PACKAGE_CONTROL_STRICT_PERMISSION ON)
include(CPack)
