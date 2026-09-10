#include "CommandTypes.h"

#include <QtTest/QtTest>

class CommandParserTests : public QObject {
    Q_OBJECT

private slots:
    void parsesEnglishJson();
    void parsesChineseAlias();
    void rejectsUnknownCommand();
    void parsesSessionSequence();
    void rejectsIncompleteSessionSequence();
    void rejectsOutOfRangeSequence();
};

void CommandParserTests::parsesEnglishJson()
{
    const QByteArray json = R"({"version":1,"request_id":"rk-1","command":"forward","value":1.5,"unit":"m/s","duration_ms":900})";
    const ParseResult result = CommandParser::parseJsonDatagram(json);
    QVERIFY2(result.ok, qPrintable(result.error));
    QCOMPARE(result.command.requestId, QStringLiteral("rk-1"));
    QCOMPARE(static_cast<int>(result.command.kind), static_cast<int>(CommandKind::Forward));
    QCOMPARE(result.command.value, 1.5);
    QCOMPARE(result.command.durationMs, 900);
}

void CommandParserTests::parsesChineseAlias()
{
    const QByteArray json = QStringLiteral(R"({"version":1,"request_id":"rk-2","command":"左转","value":30})").toUtf8();
    const ParseResult result = CommandParser::parseJsonDatagram(json);
    QVERIFY2(result.ok, qPrintable(result.error));
    QCOMPARE(static_cast<int>(result.command.kind), static_cast<int>(CommandKind::YawLeft));
}

void CommandParserTests::rejectsUnknownCommand()
{
    const QByteArray json = R"({"version":1,"request_id":"rk-3","command":"orbit"})";
    const ParseResult result = CommandParser::parseJsonDatagram(json);
    QVERIFY(!result.ok);
    QVERIFY(result.error.contains(QStringLiteral("unknown command")));
}

void CommandParserTests::parsesSessionSequence()
{
    const ParseResult result = CommandParser::parseJsonDatagram(
        R"({"version":1,"session_id":"flight-1","sequence":42,"command":"forward"})");
    QVERIFY2(result.ok, qPrintable(result.error));
    QCOMPARE(result.command.sessionId, QStringLiteral("flight-1"));
    QVERIFY(result.command.hasSequence);
    QCOMPARE(result.command.sequence, quint64(42));
}

void CommandParserTests::rejectsIncompleteSessionSequence()
{
    const ParseResult missingSequence = CommandParser::parseJsonDatagram(
        R"({"version":1,"session_id":"flight-1","command":"forward"})");
    QVERIFY(!missingSequence.ok);

    const ParseResult missingSession = CommandParser::parseJsonDatagram(
        R"({"version":1,"sequence":1,"command":"forward"})");
    QVERIFY(!missingSession.ok);
}

void CommandParserTests::rejectsOutOfRangeSequence()
{
    const ParseResult result = CommandParser::parseJsonDatagram(
        R"({"version":1,"session_id":"flight-1","sequence":1e20,"command":"forward"})");
    QVERIFY(!result.ok);
    QVERIFY(result.error.contains(QStringLiteral("positive integer")));
}

QTEST_GUILESS_MAIN(CommandParserTests)

#include "test_command_parser.moc"
