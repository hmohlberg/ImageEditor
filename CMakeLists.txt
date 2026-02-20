cmake_minimum_required(VERSION 3.22)

project(ImageEditor LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Apple Silicon optimization
if(APPLE)
  add_compile_options(-O3 -mcpu=apple-m4 -ffast-math)
endif()

# Qt6 finden
if(APPLE)
  list(APPEND CMAKE_PREFIX_PATH "/opt/homebrew")
  list(APPEND CMAKE_PREFIX_PATH "/opt/homebrew/Cellar/qtbase/6.10.1")
  set(Qt6Svg_DIR "/opt/homebrew/opt/qtsvg/lib/cmake/Qt6Svg")
  set(CMAKE_PREFIX_PATH "/opt/homebrew/opt/qtbase")
endif()
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Svg)

# itk find
if(APPLE)
  set(Qt6Quick_DIR "/opt/homebrew/opt/qtdeclarative/lib/cmake/Qt6Quick")
  set(CMAKE_PREFIX_PATH "/opt/homebrew/opt/fftw;${CMAKE_PREFIX_PATH}")
  link_directories(/opt/homebrew/lib)
endif()
# find_package(ITK QUIET)
if(ITK_FOUND)
  add_definitions(-DHASITK)
  message(STATUS "Found ITK")
else()
  message(STATUS "ITK not found")
endif()

# Quellen
set(SOURCES
    main.cpp
    core/ImageLoader.cpp
    core/ImageProcessor.cpp
    gui/MainWindow.cpp
    gui/ImageView.cpp
    layer/LayerItem.cpp
    layer/CageLayerItem.cpp
    layer/CageMesh.cpp
    layer/CageControlPointItem.cpp
    layer/CageOverlayItem.cpp
    layer/EditablePolygon.cpp
    layer/EditablePolygonItem.cpp
    layer/TransformHandleItem.cpp
    layer/PerspectiveTransform.cpp
    layer/PerspectiveOverlay.cpp
    layer/MaskLayer.cpp
    layer/MaskLayerItem.cpp
    layer/TransformOverlay.cpp
    layer/CenterHandleItem.cpp
    undo/AbstractCommand.cpp
    undo/CageMoveCommand.cpp
    undo/CageWarpCommand.cpp
    undo/DeleteLayerCommand.cpp
    undo/LassoCutCommand.cpp
    undo/PaintCommand.cpp
    undo/PaintStrokeCommand.cpp
    undo/InvertLayerCommand.cpp
    undo/MirrorLayerCommand.cpp
    undo/EditablePolygonCommand.cpp
    undo/TransformLayerCommand.cpp
    undo/PerspectiveTransformCommand.cpp
    undo/PerspectiveWarpCommand.cpp
    undo/PolygonMovePointCommand.cpp
    undo/PolygonInsertPointCommand.cpp
    undo/PolygonDeletePointCommand.cpp
    undo/PolygonTranslateCommand.cpp
    undo/PolygonSmoothCommand.cpp
    undo/PolygonReduceCommand.cpp
    undo/MoveLayerCommand.cpp
    undo/MaskStrokeCommand.cpp
    undo/MaskPaintCommand.cpp
)

set(HEADERS
    core/ImageLoader.h
    core/ImageProcessor.h
    gui/MainWindow.h
    gui/ImageView.h
    layer/LayerItem.h
    layer/CageLayerItem.h
    layer/CageMesh.h
    layer/CageControlPointItem.h
    layer/CageOverlayItem.h
    layer/LassoCutCommand.h
    layer/EditablePolygon.h
    layer/EditablePolygonItem.h
    layer/TransformHandleItem.h
    layer/PerspectiveTransform.h
    layer/PerspectiveOverlay.cpp
    layer/MaskLayer.h
    layer/MaskLayerItem.h
    layer/TransformOverlay.h
    layer/CenterHandleItem.h
    undo/AbstractCommand.h
    undo/CageMoveCommand.h
    undo/CageWarpCommand.h
    undo/DeleteLayerCommand.h
    undo/PaintCommand.h
    undo/PaintStrokeCommand.h
    undo/InvertLayerCommand.h
    undo/MaskStrokeCommand.h
    undo/MaskPaintCommand.h
    undo/MirrorLayerCommand.h
    undo/TransformLayerCommand.h
    undo/PerspectiveTransformCommand.h
    undo/PerspectiveWarpCommand.h
    undo/EditablePolygonCommand.h
    undo/PolygonMovePointCommand.h
    undo/PolygonInsertPointCommand.h
    undo/PolygonDeletePointCommand.h
    undo/PolygonTranslateCommand.h
    undo/PolygonSmoothCommand.h
    undo/PolygonReduceCommand.h
    util/GeometryUtils.h
)

# Qt6 Executable
qt_standard_project_setup()
qt_add_executable(${PROJECT_NAME} ${SOURCES})

target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Svg)
if(ITK_FOUND)
    include(${ITK_USE_FILE})
    target_include_directories(ImageEditor PRIVATE ${ITK_INCLUDE_DIRS})
    target_link_libraries(ImageEditor PRIVATE ${ITK_LIBRARIES})
    target_compile_definitions(ImageEditor PRIVATE HASITK)
endif()

# Automatische Suche nach Plugins ermöglichen
if(APPLE)
    set(QT_PLATFORMS_SRC "/opt/homebrew/opt/qtbase/share/qt/plugins/platforms")
    set(QT_PLATFORMS_DEST "${CMAKE_BINARY_DIR}/platforms")
    add_custom_command(TARGET ImageEditor POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${QT_PLATFORMS_DEST}"
        COMMAND ${CMAKE_COMMAND} -E copy "${QT_PLATFORMS_SRC}/libqcocoa.dylib" "${QT_PLATFORMS_DEST}/"
        COMMENT "Kopiere libqcocoa.dylib nach ${QT_PLATFORMS_DEST}"
    )
endif()
