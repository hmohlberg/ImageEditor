/****************************************************************************
** Meta object code from reading C++ file 'MainWindow.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.8.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../gui/MainWindow.h"
#include <QtGui/qtextcursor.h>
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'MainWindow.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 68
#error "This file was generated using the moc from 6.8.2. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

#ifndef Q_CONSTINIT
#define Q_CONSTINIT
#endif

QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
QT_WARNING_DISABLE_GCC("-Wuseless-cast")
namespace {
struct qt_meta_tag_ZN10MainWindowE_t {};
} // unnamed namespace


#ifdef QT_MOC_HAS_STRINGDATA
static constexpr auto qt_meta_stringdata_ZN10MainWindowE = QtMocHelpers::stringData(
    "MainWindow",
    "selectLayerItem",
    "",
    "itemName",
    "rebuildLayerList",
    "newLassoLayerCreated",
    "toggleLayerVisibility",
    "QListWidgetItem*",
    "item",
    "layerItemClicked",
    "onLayerItemClicked",
    "showLayerContextMenu",
    "pos",
    "deleteLayer",
    "duplicateLayer",
    "renameLayer",
    "mergeLayer",
    "zoom1to1",
    "fitToWindow",
    "openImage",
    "saveAsImage",
    "openHistory",
    "saveHistory",
    "toggleDocks",
    "createMaskImage",
    "forcedUpdate",
    "showConfig",
    "editLayer",
    "Layer*",
    "layer",
    "onLayerUpdateRequested",
    "modifiedImage",
    "openBigTiff",
    "filePath",
    "scheduleOverviewRefresh",
    "updateButtonState",
    "updateControlButtonState",
    "updatePolygonEnabledState",
    "isToggled"
);
#else  // !QT_MOC_HAS_STRINGDATA
#error "qtmochelpers.h not found or too old."
#endif // !QT_MOC_HAS_STRINGDATA

Q_CONSTINIT static const uint qt_meta_data_ZN10MainWindowE[] = {

 // content:
      12,       // revision
       0,       // classname
       0,    0, // classinfo
      28,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

 // slots: name, argc, parameters, tag, flags, initial metatype offsets
       1,    1,  182,    2, 0x0a,    1 /* Public */,
       4,    0,  185,    2, 0x08,    3 /* Private */,
       5,    0,  186,    2, 0x08,    4 /* Private */,
       6,    1,  187,    2, 0x08,    5 /* Private */,
       9,    1,  190,    2, 0x08,    7 /* Private */,
      10,    1,  193,    2, 0x08,    9 /* Private */,
      11,    1,  196,    2, 0x08,   11 /* Private */,
      13,    0,  199,    2, 0x08,   13 /* Private */,
      14,    0,  200,    2, 0x08,   14 /* Private */,
      15,    0,  201,    2, 0x08,   15 /* Private */,
      16,    0,  202,    2, 0x08,   16 /* Private */,
      17,    0,  203,    2, 0x08,   17 /* Private */,
      18,    0,  204,    2, 0x08,   18 /* Private */,
      19,    0,  205,    2, 0x08,   19 /* Private */,
      20,    0,  206,    2, 0x08,   20 /* Private */,
      21,    0,  207,    2, 0x08,   21 /* Private */,
      22,    0,  208,    2, 0x08,   22 /* Private */,
      23,    0,  209,    2, 0x08,   23 /* Private */,
      24,    0,  210,    2, 0x08,   24 /* Private */,
      25,    0,  211,    2, 0x08,   25 /* Private */,
      26,    0,  212,    2, 0x08,   26 /* Private */,
      27,    1,  213,    2, 0x08,   27 /* Private */,
      30,    1,  216,    2, 0x08,   29 /* Private */,
      32,    1,  219,    2, 0x08,   31 /* Private */,
      34,    0,  222,    2, 0x08,   33 /* Private */,
      35,    0,  223,    2, 0x08,   34 /* Private */,
      36,    0,  224,    2, 0x08,   35 /* Private */,
      37,    1,  225,    2, 0x08,   36 /* Private */,

 // slots: parameters
    QMetaType::Void, QMetaType::QString,    3,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, 0x80000000 | 7,    8,
    QMetaType::Void, 0x80000000 | 7,    8,
    QMetaType::Void, 0x80000000 | 7,    8,
    QMetaType::Void, QMetaType::QPoint,   12,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Bool,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, 0x80000000 | 28,   29,
    QMetaType::Void, QMetaType::QImage,   31,
    QMetaType::Void, QMetaType::QString,   33,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Bool,   38,

       0        // eod
};

Q_CONSTINIT const QMetaObject MainWindow::staticMetaObject = { {
    QMetaObject::SuperData::link<QMainWindow::staticMetaObject>(),
    qt_meta_stringdata_ZN10MainWindowE.offsetsAndSizes,
    qt_meta_data_ZN10MainWindowE,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_tag_ZN10MainWindowE_t,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<MainWindow, std::true_type>,
        // method 'selectLayerItem'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'rebuildLayerList'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'newLassoLayerCreated'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'toggleLayerVisibility'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<QListWidgetItem *, std::false_type>,
        // method 'layerItemClicked'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<QListWidgetItem *, std::false_type>,
        // method 'onLayerItemClicked'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<QListWidgetItem *, std::false_type>,
        // method 'showLayerContextMenu'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QPoint &, std::false_type>,
        // method 'deleteLayer'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'duplicateLayer'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'renameLayer'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'mergeLayer'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'zoom1to1'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'fitToWindow'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'openImage'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'saveAsImage'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'openHistory'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'saveHistory'
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        // method 'toggleDocks'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'createMaskImage'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'forcedUpdate'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'showConfig'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'editLayer'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<Layer *, std::false_type>,
        // method 'onLayerUpdateRequested'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QImage &, std::false_type>,
        // method 'openBigTiff'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'scheduleOverviewRefresh'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'updateButtonState'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'updateControlButtonState'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'updatePolygonEnabledState'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>
    >,
    nullptr
} };

void MainWindow::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<MainWindow *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->selectLayerItem((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 1: _t->rebuildLayerList(); break;
        case 2: _t->newLassoLayerCreated(); break;
        case 3: _t->toggleLayerVisibility((*reinterpret_cast< std::add_pointer_t<QListWidgetItem*>>(_a[1]))); break;
        case 4: _t->layerItemClicked((*reinterpret_cast< std::add_pointer_t<QListWidgetItem*>>(_a[1]))); break;
        case 5: _t->onLayerItemClicked((*reinterpret_cast< std::add_pointer_t<QListWidgetItem*>>(_a[1]))); break;
        case 6: _t->showLayerContextMenu((*reinterpret_cast< std::add_pointer_t<QPoint>>(_a[1]))); break;
        case 7: _t->deleteLayer(); break;
        case 8: _t->duplicateLayer(); break;
        case 9: _t->renameLayer(); break;
        case 10: _t->mergeLayer(); break;
        case 11: _t->zoom1to1(); break;
        case 12: _t->fitToWindow(); break;
        case 13: _t->openImage(); break;
        case 14: _t->saveAsImage(); break;
        case 15: _t->openHistory(); break;
        case 16: { bool _r = _t->saveHistory();
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = std::move(_r); }  break;
        case 17: _t->toggleDocks(); break;
        case 18: _t->createMaskImage(); break;
        case 19: _t->forcedUpdate(); break;
        case 20: _t->showConfig(); break;
        case 21: _t->editLayer((*reinterpret_cast< std::add_pointer_t<Layer*>>(_a[1]))); break;
        case 22: _t->onLayerUpdateRequested((*reinterpret_cast< std::add_pointer_t<QImage>>(_a[1]))); break;
        case 23: _t->openBigTiff((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 24: _t->scheduleOverviewRefresh(); break;
        case 25: _t->updateButtonState(); break;
        case 26: _t->updateControlButtonState(); break;
        case 27: _t->updatePolygonEnabledState((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1]))); break;
        default: ;
        }
    }
}

const QMetaObject *MainWindow::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *MainWindow::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_ZN10MainWindowE.stringdata0))
        return static_cast<void*>(this);
    if (!strcmp(_clname, "IMainSystem"))
        return static_cast< IMainSystem*>(this);
    return QMainWindow::qt_metacast(_clname);
}

int MainWindow::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QMainWindow::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 28)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 28;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 28)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 28;
    }
    return _id;
}
QT_WARNING_POP
