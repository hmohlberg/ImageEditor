/****************************************************************************
** Meta object code from reading C++ file 'LayerEditorView.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.8.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../gui/LayerEditorView.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'LayerEditorView.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN15LayerEditorViewE_t {};
} // unnamed namespace


#ifdef QT_MOC_HAS_STRINGDATA
static constexpr auto qt_meta_stringdata_ZN15LayerEditorViewE = QtMocHelpers::stringData(
    "LayerEditorView",
    "quitRequested",
    "",
    "updateRequested",
    "modifiedImage",
    "pickerSampled",
    "x",
    "y",
    "color",
    "text",
    "scaleChanged",
    "scale",
    "zoomIn",
    "zoomOut",
    "centerImage",
    "toggleView",
    "toggleEraser",
    "active",
    "togglePicker",
    "onUpdateClicked"
);
#else  // !QT_MOC_HAS_STRINGDATA
#error "qtmochelpers.h not found or too old."
#endif // !QT_MOC_HAS_STRINGDATA

Q_CONSTINIT static const uint qt_meta_data_ZN15LayerEditorViewE[] = {

 // content:
      12,       // revision
       0,       // classname
       0,    0, // classinfo
      11,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       4,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    0,   80,    2, 0x06,    1 /* Public */,
       3,    1,   81,    2, 0x06,    2 /* Public */,
       5,    4,   84,    2, 0x06,    4 /* Public */,
      10,    1,   93,    2, 0x06,    9 /* Public */,

 // slots: name, argc, parameters, tag, flags, initial metatype offsets
      12,    0,   96,    2, 0x08,   11 /* Private */,
      13,    0,   97,    2, 0x08,   12 /* Private */,
      14,    0,   98,    2, 0x08,   13 /* Private */,
      15,    0,   99,    2, 0x08,   14 /* Private */,
      16,    1,  100,    2, 0x08,   15 /* Private */,
      18,    1,  103,    2, 0x08,   17 /* Private */,
      19,    0,  106,    2, 0x08,   19 /* Private */,

 // signals: parameters
    QMetaType::Void,
    QMetaType::Void, QMetaType::QImage,    4,
    QMetaType::Void, QMetaType::Int, QMetaType::Int, QMetaType::QColor, QMetaType::QString,    6,    7,    8,    9,
    QMetaType::Void, QMetaType::Double,   11,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Bool,   17,
    QMetaType::Void, QMetaType::Bool,   17,
    QMetaType::Void,

       0        // eod
};

Q_CONSTINIT const QMetaObject LayerEditorView::staticMetaObject = { {
    QMetaObject::SuperData::link<QWidget::staticMetaObject>(),
    qt_meta_stringdata_ZN15LayerEditorViewE.offsetsAndSizes,
    qt_meta_data_ZN15LayerEditorViewE,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_tag_ZN15LayerEditorViewE_t,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<LayerEditorView, std::true_type>,
        // method 'quitRequested'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'updateRequested'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QImage &, std::false_type>,
        // method 'pickerSampled'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QColor &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'scaleChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<double, std::false_type>,
        // method 'zoomIn'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'zoomOut'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'centerImage'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'toggleView'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'toggleEraser'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        // method 'togglePicker'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        // method 'onUpdateClicked'
        QtPrivate::TypeAndForceComplete<void, std::false_type>
    >,
    nullptr
} };

void LayerEditorView::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<LayerEditorView *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->quitRequested(); break;
        case 1: _t->updateRequested((*reinterpret_cast< std::add_pointer_t<QImage>>(_a[1]))); break;
        case 2: _t->pickerSampled((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<QColor>>(_a[3])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[4]))); break;
        case 3: _t->scaleChanged((*reinterpret_cast< std::add_pointer_t<double>>(_a[1]))); break;
        case 4: _t->zoomIn(); break;
        case 5: _t->zoomOut(); break;
        case 6: _t->centerImage(); break;
        case 7: _t->toggleView(); break;
        case 8: _t->toggleEraser((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1]))); break;
        case 9: _t->togglePicker((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1]))); break;
        case 10: _t->onUpdateClicked(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _q_method_type = void (LayerEditorView::*)();
            if (_q_method_type _q_method = &LayerEditorView::quitRequested; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 0;
                return;
            }
        }
        {
            using _q_method_type = void (LayerEditorView::*)(const QImage & );
            if (_q_method_type _q_method = &LayerEditorView::updateRequested; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 1;
                return;
            }
        }
        {
            using _q_method_type = void (LayerEditorView::*)(int , int , const QColor & , const QString & );
            if (_q_method_type _q_method = &LayerEditorView::pickerSampled; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 2;
                return;
            }
        }
        {
            using _q_method_type = void (LayerEditorView::*)(double );
            if (_q_method_type _q_method = &LayerEditorView::scaleChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 3;
                return;
            }
        }
    }
}

const QMetaObject *LayerEditorView::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *LayerEditorView::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_ZN15LayerEditorViewE.stringdata0))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int LayerEditorView::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 11)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 11;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 11)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 11;
    }
    return _id;
}

// SIGNAL 0
void LayerEditorView::quitRequested()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void LayerEditorView::updateRequested(const QImage & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 1, _a);
}

// SIGNAL 2
void LayerEditorView::pickerSampled(int _t1, int _t2, const QColor & _t3, const QString & _t4)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t3))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t4))) };
    QMetaObject::activate(this, &staticMetaObject, 2, _a);
}

// SIGNAL 3
void LayerEditorView::scaleChanged(double _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 3, _a);
}
QT_WARNING_POP
