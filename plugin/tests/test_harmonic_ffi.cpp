#include "harmonic.h"

#include <QByteArray>
#include <QtTest/QtTest>

class HarmonicFfiTest : public QObject {
    Q_OBJECT

  private Q_SLOTS:
    void nullPromptReturnsInvalidArgument();
    void unknownBackendReturnsInvalidBackend();
    void nonexistentCommandReturnsSpawnFailed();
    void legacyApiReturnsErrorPrefix();
};

void HarmonicFfiTest::nullPromptReturnsInvalidArgument() {
    HarmonicResult result = harmonic_generate_result(
        nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, true);
    QCOMPARE(result.status, HARMONIC_STATUS_INVALID_ARGUMENT);
    QVERIFY(result.output == nullptr);
    QVERIFY(result.error != nullptr);
    harmonic_free_result(result);
}

void HarmonicFfiTest::unknownBackendReturnsInvalidBackend() {
    const QByteArray prompt("hello");
    const QByteArray backend("not-a-backend");
    HarmonicResult result = harmonic_generate_result(
        prompt.constData(), nullptr, backend.constData(), nullptr, nullptr, nullptr, true);
    QCOMPARE(result.status, HARMONIC_STATUS_INVALID_BACKEND);
    QVERIFY(result.output == nullptr);
    QVERIFY(result.error != nullptr);
    harmonic_free_result(result);
}

void HarmonicFfiTest::nonexistentCommandReturnsSpawnFailed() {
    const QByteArray prompt("hello");
    const QByteArray backend("opencode");
    const QByteArray command("__harmonic_nonexistent_backend__");
    HarmonicResult result = harmonic_generate_result(
        prompt.constData(), nullptr, backend.constData(), command.constData(), nullptr, nullptr, true);
    QCOMPARE(result.status, HARMONIC_STATUS_SPAWN_FAILED);
    QVERIFY(result.output == nullptr);
    QVERIFY(result.error != nullptr);
    harmonic_free_result(result);
}

void HarmonicFfiTest::legacyApiReturnsErrorPrefix() {
    const QByteArray prompt("hello");
    const QByteArray backend("not-a-backend");
    char *result = harmonic_generate(
        prompt.constData(), nullptr, backend.constData(), nullptr, nullptr, nullptr, true);
    QVERIFY(result != nullptr);
    const QByteArray text(result);
    QVERIFY(text.startsWith("// Error: "));
    harmonic_free_string(result);
}

QTEST_MAIN(HarmonicFfiTest)
#include "test_harmonic_ffi.moc"
