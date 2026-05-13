/****************************************************************************
** Meta object code from reading C++ file 'harmonicplugin.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.10.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../harmonicplugin.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'harmonicplugin.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 69
#error "This file was generated using the moc from 6.10.2. It"
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
struct qt_meta_tag_ZN14HarmonicPluginE_t {};
} // unnamed namespace

template <> constexpr inline auto HarmonicPlugin::qt_create_metaobjectdata<qt_meta_tag_ZN14HarmonicPluginE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "HarmonicPlugin"
    };

    QtMocHelpers::UintData qt_methods {
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<HarmonicPlugin, qt_meta_tag_ZN14HarmonicPluginE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject HarmonicPlugin::staticMetaObject = { {
    QMetaObject::SuperData::link<KTextEditor::Plugin::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN14HarmonicPluginE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN14HarmonicPluginE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN14HarmonicPluginE_t>.metaTypes,
    nullptr
} };

void HarmonicPlugin::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<HarmonicPlugin *>(_o);
    (void)_t;
    (void)_c;
    (void)_id;
    (void)_a;
}

const QMetaObject *HarmonicPlugin::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *HarmonicPlugin::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN14HarmonicPluginE_t>.strings))
        return static_cast<void*>(this);
    return KTextEditor::Plugin::qt_metacast(_clname);
}

int HarmonicPlugin::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = KTextEditor::Plugin::qt_metacall(_c, _id, _a);
    return _id;
}
namespace {
struct qt_meta_tag_ZN12HarmonicViewE_t {};
} // unnamed namespace

template <> constexpr inline auto HarmonicView::qt_create_metaobjectdata<qt_meta_tag_ZN12HarmonicViewE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "HarmonicView",
        "vibecode",
        "",
        "updateChatContext"
    };

    QtMocHelpers::UintData qt_methods {
        // Slot 'vibecode'
        QtMocHelpers::SlotData<void()>(1, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'updateChatContext'
        QtMocHelpers::SlotData<void()>(3, 2, QMC::AccessPrivate, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<HarmonicView, qt_meta_tag_ZN12HarmonicViewE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject HarmonicView::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN12HarmonicViewE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN12HarmonicViewE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN12HarmonicViewE_t>.metaTypes,
    nullptr
} };

void HarmonicView::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<HarmonicView *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->vibecode(); break;
        case 1: _t->updateChatContext(); break;
        default: ;
        }
    }
    (void)_a;
}

const QMetaObject *HarmonicView::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *HarmonicView::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN12HarmonicViewE_t>.strings))
        return static_cast<void*>(this);
    if (!strcmp(_clname, "KXMLGUIClient"))
        return static_cast< KXMLGUIClient*>(this);
    return QObject::qt_metacast(_clname);
}

int HarmonicView::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 2)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 2;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 2)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 2;
    }
    return _id;
}
QT_WARNING_POP
