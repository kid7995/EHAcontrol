cmake_minimum_required(VERSION 3.16)

project(KaiserStudio VERSION 0.2 LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets SerialPort Charts)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets SerialPort Charts)

set(PROJECT_SOURCES
    main.cpp
    mainwindow.cpp
    mainwindow.h
    deviceserialservice.cpp
    deviceserialservice.h
    parametercatalog.cpp
    parametercatalog.h
    parametermodel.cpp
    parametermodel.h
    dashboardpage.cpp
    dashboardpage.h
    parameterpage.cpp
    parameterpage.h
    monitorpage.cpp
    monitorpage.h
    communicationpage.cpp
    communicationpage.h
    driveinfopage.cpp
    driveinfopage.h
    faultpage.cpp
    faultpage.h
    feedbackpage.cpp
    feedbackpage.h
    deviceconfigpage.cpp
    deviceconfigpage.h
    digitaliopage.cpp
    digitaliopage.h
    homingpage.cpp
    homingpage.h
    icon.qrc
    image.qrc
    reference_assets.qrc
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(KaiserStudio
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
else()
    add_executable(KaiserStudio ${PROJECT_SOURCES})
endif()

target_link_libraries(KaiserStudio PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::SerialPort
    Qt${QT_VERSION_MAJOR}::Charts
)

set_target_properties(KaiserStudio PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
)

include(GNUInstallDirs)
install(TARGETS KaiserStudio
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(KaiserStudio)
endif()
