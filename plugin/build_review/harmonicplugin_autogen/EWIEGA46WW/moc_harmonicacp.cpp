/****************************************************************************
** Meta object code from reading C++ file 'harmonicacp.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.10.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../harmonicacp.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'harmonicacp.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN11HarmonicAcpE_t {};
} // unnamed namespace

template <> constexpr inline auto HarmonicAcp::qt_create_metaobjectdata<qt_meta_tag_ZN11HarmonicAcpE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "HarmonicAcp",
        "initialized",
        "",
        "QJsonObject",
        "agentInfo",
        "sessionCreated",
        "sessionId",
        "textChunk",
        "text",
        "thoughtChunk",
        "toolCall",
        "toolCallId",
        "title",
        "kind",
        "toolCallUpdate",
        "status",
        "content",
        "permissionRequested",
        "QJsonValue",
        "requestId",
        "QJsonArray",
        "options",
        "promptFinished",
        "stopReason",
        "errorOccurred",
        "message",
        "processFinished",
        "onProcessStarted",
        "onReadyRead",
        "onProcessFinished",
        "exitCode",
        "QProcess::ExitStatus",
        "onProcessError",
        "QProcess::ProcessError",
        "error"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'initialized'
        QtMocHelpers::SignalData<void(const QJsonObject &)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 3, 4 },
        }}),
        // Signal 'sessionCreated'
        QtMocHelpers::SignalData<void(const QString &)>(5, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 6 },
        }}),
        // Signal 'textChunk'
        QtMocHelpers::SignalData<void(const QString &)>(7, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 8 },
        }}),
        // Signal 'thoughtChunk'
        QtMocHelpers::SignalData<void(const QString &)>(9, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 8 },
        }}),
        // Signal 'toolCall'
        QtMocHelpers::SignalData<void(const QString &, const QString &, const QString &)>(10, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 11 }, { QMetaType::QString, 12 }, { QMetaType::QString, 13 },
        }}),
        // Signal 'toolCallUpdate'
        QtMocHelpers::SignalData<void(const QString &, const QString &, const QString &)>(14, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 11 }, { QMetaType::QString, 15 }, { QMetaType::QString, 16 },
        }}),
        // Signal 'permissionRequested'
        QtMocHelpers::SignalData<void(const QJsonValue &, const QString &, const QJsonArray &)>(17, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 18, 19 }, { QMetaType::QString, 12 }, { 0x80000000 | 20, 21 },
        }}),
        // Signal 'promptFinished'
        QtMocHelpers::SignalData<void(const QString &)>(22, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 23 },
        }}),
        // Signal 'errorOccurred'
        QtMocHelpers::SignalData<void(const QString &)>(24, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 25 },
        }}),
        // Signal 'processFinished'
        QtMocHelpers::SignalData<void()>(26, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'onProcessStarted'
        QtMocHelpers::SlotData<void()>(27, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onReadyRead'
        QtMocHelpers::SlotData<void()>(28, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onProcessFinished'
        QtMocHelpers::SlotData<void(int, QProcess::ExitStatus)>(29, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Int, 30 }, { 0x80000000 | 31, 15 },
        }}),
        // Slot 'onProcessError'
        QtMocHelpers::SlotData<void(QProcess::ProcessError)>(32, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 33, 34 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<HarmonicAcp, qt_meta_tag_ZN11HarmonicAcpE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject HarmonicAcp::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN11HarmonicAcpE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN11HarmonicAcpE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN11HarmonicAcpE_t>.metaTypes,
    nullptr
} };

void HarmonicAcp::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<HarmonicAcp *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->initialized((*reinterpret_cast<std::add_pointer_t<QJsonObject>>(_a[1]))); break;
        case 1: _t->sessionCreated((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 2: _t->textChunk((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 3: _t->thoughtChunk((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 4: _t->toolCall((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[3]))); break;
        case 5: _t->toolCallUpdate((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[3]))); break;
        case 6: _t->permissionRequested((*reinterpret_cast<std::add_pointer_t<QJsonValue>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<QJsonArray>>(_a[3]))); break;
        case 7: _t->promptFinished((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 8: _t->errorOccurred((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 9: _t->processFinished(); break;
        case 10: _t->onProcessStarted(); break;
        case 11: _t->onReadyRead(); break;
        case 12: _t->onProcessFinished((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QProcess::ExitStatus>>(_a[2]))); break;
        case 13: _t->onProcessError((*reinterpret_cast<std::add_pointer_t<QProcess::ProcessError>>(_a[1]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (HarmonicAcp::*)(const QJsonObject & )>(_a, &HarmonicAcp::initialized, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (HarmonicAcp::*)(const QString & )>(_a, &HarmonicAcp::sessionCreated, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (HarmonicAcp::*)(const QString & )>(_a, &HarmonicAcp::textChunk, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (HarmonicAcp::*)(const QString & )>(_a, &HarmonicAcp::thoughtChunk, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (HarmonicAcp::*)(const QString & , const QString & , const QString & )>(_a, &HarmonicAcp::toolCall, 4))
            return;
        if (QtMocHelpers::indexOfMethod<void (HarmonicAcp::*)(const QString & , const QString & , const QString & )>(_a, &HarmonicAcp::toolCallUpdate, 5))
            return;
        if (QtMocHelpers::indexOfMethod<void (HarmonicAcp::*)(const QJsonValue & , const QString & , const QJsonArray & )>(_a, &HarmonicAcp::permissionRequested, 6))
            return;
        if (QtMocHelpers::indexOfMethod<void (HarmonicAcp::*)(const QString & )>(_a, &HarmonicAcp::promptFinished, 7))
            return;
        if (QtMocHelpers::indexOfMethod<void (HarmonicAcp::*)(const QString & )>(_a, &HarmonicAcp::errorOccurred, 8))
            return;
        if (QtMocHelpers::indexOfMethod<void (HarmonicAcp::*)()>(_a, &HarmonicAcp::processFinished, 9))
            return;
    }
}

const QMetaObject *HarmonicAcp::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *HarmonicAcp::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN11HarmonicAcpE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int HarmonicAcp::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 14)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 14;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 14)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 14;
    }
    return _id;
}

// SIGNAL 0
void HarmonicAcp::initialized(const QJsonObject & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1);
}

// SIGNAL 1
void HarmonicAcp::sessionCreated(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1);
}

// SIGNAL 2
void HarmonicAcp::textChunk(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 2, nullptr, _t1);
}

// SIGNAL 3
void HarmonicAcp::thoughtChunk(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 3, nullptr, _t1);
}

// SIGNAL 4
void HarmonicAcp::toolCall(const QString & _t1, const QString & _t2, const QString & _t3)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 4, nullptr, _t1, _t2, _t3);
}

// SIGNAL 5
void HarmonicAcp::toolCallUpdate(const QString & _t1, const QString & _t2, const QString & _t3)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 5, nullptr, _t1, _t2, _t3);
}

// SIGNAL 6
void HarmonicAcp::permissionRequested(const QJsonValue & _t1, const QString & _t2, const QJsonArray & _t3)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 6, nullptr, _t1, _t2, _t3);
}

// SIGNAL 7
void HarmonicAcp::promptFinished(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 7, nullptr, _t1);
}

// SIGNAL 8
void HarmonicAcp::errorOccurred(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 8, nullptr, _t1);
}

// SIGNAL 9
void HarmonicAcp::processFinished()
{
    QMetaObject::activate(this, &staticMetaObject, 9, nullptr);
}
QT_WARNING_POP
