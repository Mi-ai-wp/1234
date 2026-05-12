cmake_minimum_required(VERSION 3.19)
project(111 LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets)

qt_standard_project_setup()

qt_add_executable(111
    WIN32 MACOSX_BUNDLE
    main.cpp
    gamewindow.cpp
    gamewindow.h
    gamewidget.cpp
    gamewidget.h
)

qt_add_resources(111 "resources"
    PREFIX "/images"
    FILES
        tp/1.jpg
        tp/2.jpg
        tp/3.jpg
        tp/4.jpg
        tp/5.jpg
        tp/6.jpg
        tp/7.jpg
        tp/8.jpg
        tp/9.jpg
        tp/10.jpg
        tp/11.jpg
)

target_link_libraries(111
    PRIVATE
        Qt::Core
        Qt::Widgets
)

install(TARGETS 111
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET 111
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})