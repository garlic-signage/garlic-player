/*
 garlic-player: SMIL Player for Digital Signage
 Copyright (C) 2026 Nikolaos Sagiadinos <garlic@saghiadinos.de>
 This file is part of the GarlicSignage source code

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU Affero General Public License, version 3,
 as published by the Free Software Foundation.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU Affero General Public License for more details.

 You should have received a copy of the GNU Affero General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "supervisor_client.hpp"

#include "player_runtime/player_runtime_api.hpp"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLocalServer>
#include <QLocalSocket>
#include <QSignalSpy>
#include <QTemporaryFile>
#include <QTest>
#include <QUuid>

#include <functional>
#include <thread>

using namespace std::chrono_literals;
using player::SupervisorClient;

namespace
{
    /**
     * The supervisor side for the tests: listens, answers hello with hello_result (unless answer_hello is false) and
     * keeps every message of the player.
     */
    class FakeSupervisor final : public QObject
    {
        public:
            QLocalServer       server;
            QLocalSocket      *client = nullptr;
            QList<QJsonObject> received;
            QJsonObject        hello_result{{"protocol", 1},
                                            {"supervisor", QJsonObject{{"name", "fake"}, {"version", "0.1"}}},
                                            {"capabilities", QJsonArray{"reboot", "install", "screen"}},
                                            {"heartbeat_interval", 0}};
            bool               answer_hello = true;
            int                connections  = 0;

            FakeSupervisor()
            {
                QString name = "garlic-test-" + QUuid::createUuid().toString(QUuid::WithoutBraces);
                QLocalServer::removeServer(name);
                connect(&server, &QLocalServer::newConnection, this, [this] { accept(); });
                if (!server.listen(name))
                    qFatal("fake supervisor cannot listen");
            }

            QString endpoint() const { return server.fullServerName(); }

            void send(const QJsonObject &message) { sendRaw(QJsonDocument(message).toJson(QJsonDocument::Compact)); }

            void sendRaw(const QByteArray &line)
            {
                client->write(line + '\n');
                client->flush();
            }

            /// The first message of the player since index from that matches, or an empty object after timeout.
            QJsonObject waitFor(const std::function<bool(const QJsonObject &)> &match, qsizetype from = 0,
                                std::chrono::milliseconds timeout = 2s)
            {
                QDeadlineTimer deadline(timeout);
                while (true)
                {
                    for (qsizetype i = from; i < received.size(); ++i)
                        if (match(received[i]))
                            return received[i];
                    if (deadline.hasExpired())
                        return {};
                    QTest::qWait(10);
                }
            }

            QJsonObject waitForMethod(const QString &method, qsizetype from = 0, std::chrono::milliseconds timeout = 2s)
            {
                return waitFor([&](const QJsonObject &m) { return m.value("method").toString() == method; }, from,
                               timeout);
            }

        private:
            QByteArray buffer;

            void accept()
            {
                client = server.nextPendingConnection();
                ++connections;
                buffer.clear();
                connect(client, &QLocalSocket::readyRead, this,
                        [this, socket = client]
                        {
                            buffer += socket->readAll();
                            qsizetype end;
                            while ((end = buffer.indexOf('\n')) >= 0)
                            {
                                QJsonObject message = QJsonDocument::fromJson(buffer.left(end)).object();
                                buffer.remove(0, end + 1);
                                received << message;
                                if (message.value("method") == "hello" && answer_hello)
                                    send({{"jsonrpc", "2.0"}, {"id", message.value("id")}, {"result", hello_result}});
                            }
                        });
            }
    };

    /// Every key of expected is in actual with the same value; objects are compared the same way.
    bool contains(const QJsonObject &actual, const QJsonObject &expected)
    {
        for (auto it = expected.begin(); it != expected.end(); ++it)
        {
            if (!actual.contains(it.key()))
                return false;
            QJsonValue value = actual.value(it.key());
            if (it.value().isObject())
            {
                if (!value.isObject() || !contains(value.toObject(), it.value().toObject()))
                    return false;
            }
            else if (value != it.value())
                return false;
        }
        return true;
    }

    bool isAnswer(const QJsonObject &message)
    {
        return message.contains("result") || message.contains("error");
    }
}

class TestSupervisorClient final : public QObject
{
        Q_OBJECT

    private Q_SLOTS:
        void helloAnnouncesThePlayerAndReadsTheSupervisor()
        {
            FakeSupervisor fake;
            fake.hello_result.insert("locked", true);
            fake.hello_result.insert("device", QJsonObject{{"uuid", "u-1"}, {"content_url", "https://hub/index.smil"},
                                                           {"player_name", "Lobby"}});
            SupervisorClient client(fake.endpoint(), "2.0.0");
            QVERIFY(client.start(2s));

            QJsonObject hello = fake.received.value(0);
            QCOMPARE(hello.value("method").toString(), QString("hello"));
            QJsonObject params = hello.value("params").toObject();
            QCOMPARE(params.value("protocol").toInt(), 1);
            QCOMPARE(params.value("client").toString(), QString("player"));
            QCOMPARE(params.value("player").toObject().value("version").toString(), QString("2.0.0"));
            QVERIFY(params.value("capabilities").toArray().contains("setIndex"));

            QCOMPARE(client.hello().name, QString("fake"));
            QVERIFY(client.offers("reboot"));
            QVERIFY(!client.offers("deep_standby"));
            QVERIFY(client.locked());
            QCOMPARE(client.hello().uuid, QString("u-1"));
            QCOMPARE(client.hello().content_url, QString("https://hub/index.smil"));
            QCOMPARE(client.hello().player_name, QString("Lobby"));

            garlic::core::PlayerRuntimeConfig config;
            client.seed(config);
            QCOMPARE(QString::fromStdString(config.uuid), QString("u-1"));
        }

        void withoutSupervisorThePlayerStartsAnyway()
        {
            SupervisorClient client("garlic-test-nobody-listens", "2.0.0");
            QVERIFY(!client.start(200ms));
            QVERIFY(!client.ready());
            QVERIFY(!client.offers("reboot"));
            client.screenOff(); // only a warning
        }

        /// supervisor_protocol/player.jsonl: what every player answers to what a supervisor sends
        void casesOfTheProtocol()
        {
            QFile cases(SUPERVISOR_PROTOCOL_CASES);
            QVERIFY2(cases.open(QIODevice::ReadOnly), SUPERVISOR_PROTOCOL_CASES);
            FakeSupervisor   fake;
            SupervisorClient client(fake.endpoint(), "2.0.0");
            QVERIFY(client.start(2s));

            int count = 0;
            while (!cases.atEnd())
            {
                QByteArray line = cases.readLine().trimmed();
                if (line.isEmpty())
                    continue;
                QJsonObject test  = QJsonDocument::fromJson(line).object();
                QByteArray  title = test.value("case").toString().toUtf8();
                qsizetype   from  = fake.received.size();
                if (test.contains("send_raw"))
                    fake.sendRaw(test.value("send_raw").toString().toUtf8());
                else
                    fake.send(test.value("send").toObject());

                if (test.value("expect").isNull())
                {
                    QTest::qWait(100);
                    QVERIFY2(fake.waitFor(isAnswer, from, 0ms).isEmpty(), title.constData());
                }
                else
                {
                    QJsonObject answer = fake.waitFor(isAnswer, from);
                    QVERIFY2(!answer.isEmpty(), title.constData());
                    QVERIFY2(contains(answer, test.value("expect").toObject()),
                             (title + ": " + QJsonDocument(answer).toJson(QJsonDocument::Compact)).constData());
                }
                ++count;
            }
            QVERIFY(count >= 10);
        }

        void requestsBeforeHelloAreNotReady()
        {
            FakeSupervisor fake;
            fake.answer_hello = false;
            SupervisorClient client(fake.endpoint(), "2.0.0");
            QVERIFY(!client.start(300ms));
            fake.send({{"jsonrpc", "2.0"}, {"id", 1}, {"method", "quit"}});
            QJsonObject answer = fake.waitFor(isAnswer);
            QCOMPARE(answer.value("error").toObject().value("code").toInt(), -32001);
        }

        void requestsOfTheSupervisorReachThePlayer()
        {
            FakeSupervisor   fake;
            SupervisorClient client(fake.endpoint(), "2.0.0");
            QVERIFY(client.start(2s));
            QTemporaryFile file;
            QVERIFY(file.open());

            QSignalSpy configuration(&client, &SupervisorClient::configurationRequested);
            QSignalSpy index(&client, &SupervisorClient::indexRequested);
            QSignalSpy quit(&client, &SupervisorClient::quitRequested);
            fake.send({{"jsonrpc", "2.0"}, {"id", 1}, {"method", "applyConfiguration"},
                       {"params", QJsonObject{{"path", file.fileName()}}}});
            fake.send({{"jsonrpc", "2.0"}, {"id", 2}, {"method", "setIndex"},
                       {"params", QJsonObject{{"uri", "file://" + file.fileName()}}}});
            fake.send({{"jsonrpc", "2.0"}, {"id", 3}, {"method", "quit"}});

            QTRY_COMPARE(quit.size(), 1);
            QCOMPARE(configuration.value(0).value(0).toString(), file.fileName());
            QCOMPARE(index.value(0).value(0).toString(), file.fileName()); // file:// is removed
        }

        void commandsGoOutOnlyWithTheirCapability()
        {
            FakeSupervisor   fake;
            SupervisorClient client(fake.endpoint(), "2.0.0");
            QVERIFY(client.start(2s));

            client.deepStandby(30s); // not offered
            client.screenOff();
            QJsonObject screen = fake.waitForMethod("screenOff");
            QVERIFY(!screen.isEmpty());
            QVERIFY(screen.contains("id")); // a request, the supervisor answers
            QVERIFY(fake.waitForMethod("deepStandby", 0, 0ms).isEmpty());

            std::thread core([&client] { client.reboot("task-7"); }); // from the core thread
            core.join();
            QJsonObject reboot = fake.waitForMethod("reboot");
            QCOMPARE(reboot.value("params").toObject().value("task_id").toString(), QString("task-7"));

            client.installSoftware("/cache/garlic-player.tar.xz");
            QCOMPARE(fake.waitForMethod("installSoftware").value("params").toObject().value("path").toString(),
                     QString("/cache/garlic-player.tar.xz"));
        }

        void lockFromHelloAndSetLocked()
        {
            FakeSupervisor fake;
            fake.hello_result.insert("locked", true);
            SupervisorClient client(fake.endpoint(), "2.0.0");
            QSignalSpy       changes(&client, &SupervisorClient::lockedChanged);
            QVERIFY(client.start(2s));
            QVERIFY(client.locked());
            QCOMPARE(changes.size(), 1);

            fake.send({{"jsonrpc", "2.0"}, {"method", "setLocked"}, {"params", QJsonObject{{"locked", false}}}});
            QTRY_COMPARE(changes.size(), 2);
            QVERIFY(!client.locked());
            QCOMPARE(changes.value(1).value(0).toBool(), false);
        }

        void closingIsWrittenBeforeItReturns()
        {
            FakeSupervisor   fake;
            SupervisorClient client(fake.endpoint(), "2.0.0");
            QVERIFY(client.start(2s));
            client.closing("user");
            QJsonObject closing = fake.waitForMethod("closing");
            QCOMPARE(closing.value("params").toObject().value("reason").toString(), QString("user"));
            QVERIFY(!closing.contains("id")); // a notification
        }

        /// A probe that never comes back from the core thread: after the first heartbeat no further one.
        void heartbeatOnlyWhileTheCoreAnswers()
        {
            FakeSupervisor fake;
            fake.hello_result.insert("heartbeat_interval", 1);
            SupervisorClient client(fake.endpoint(), "2.0.0");
            bool             core_answers = true;
            client.setCoreProbe(
                [&core_answers](std::function<void()> done)
                {
                    if (core_answers)
                        done();
                });
            QVERIFY(client.start(2s));
            auto count = [&fake]
            {
                return std::ranges::count_if(fake.received, [](const QJsonObject &m)
                                             { return m.value("method").toString() == "heartbeat"; });
            };
            QTRY_VERIFY_WITH_TIMEOUT(count() >= 3, 4000);
            QVERIFY(fake.received.last().value("params").toObject().contains("uptime"));

            core_answers = false;
            QTest::qWait(1100); // a tick that still sends (the last probe came back), then no more
            auto stalled = count();
            QTest::qWait(2500);
            QCOMPARE(count(), stalled);
        }

        void aLostConnectionIsTriedAgainWithHello()
        {
            FakeSupervisor   fake;
            SupervisorClient client(fake.endpoint(), "2.0.0");
            QSignalSpy       answered(&client, &SupervisorClient::helloAnswered);
            QVERIFY(client.start(2s));
            fake.client->disconnectFromServer();
            QTRY_VERIFY(!client.ready());

            QTRY_COMPARE_WITH_TIMEOUT(fake.connections, 2, 7000);
            QTRY_COMPARE(answered.size(), 2);
            QVERIFY(client.ready());
        }

        void deviceSettingsOnlyWithTheirCapability()
        {
            FakeSupervisor fake;
            fake.hello_result.insert("capabilities", QJsonArray{"volume"});
            SupervisorClient client(fake.endpoint(), "2.0.0");
            QVERIFY(client.start(2s));
            client.deviceSettings({80, 50});
            QCOMPARE(fake.waitForMethod("setVolume").value("params").toObject().value("level").toInt(), 80);
            QVERIFY(fake.waitForMethod("setBrightness", 0, 300ms).isEmpty()); // not offered

            qsizetype from = fake.received.size();
            client.deviceSettings({std::nullopt, 50}); // no volume set: nothing to send
            QVERIFY(fake.waitForMethod("setVolume", from, 300ms).isEmpty());
        }

        /// Values from before the connection, and again after a reconnect: the supervisor always has them.
        void deviceSettingsAfterEveryHello()
        {
            FakeSupervisor fake;
            fake.hello_result.insert("capabilities", QJsonArray{"volume", "brightness"});
            SupervisorClient client(fake.endpoint(), "2.0.0");
            client.deviceSettings({40, 70}); // the core announces them before the client is connected
            QVERIFY(client.start(2s));
            QCOMPARE(fake.waitForMethod("setVolume").value("params").toObject().value("level").toInt(), 40);
            QCOMPARE(fake.waitForMethod("setBrightness").value("params").toObject().value("level").toInt(), 70);

            qsizetype from = fake.received.size();
            fake.client->disconnectFromServer();
            QTRY_COMPARE_WITH_TIMEOUT(fake.connections, 2, 7000);
            QVERIFY(!fake.waitForMethod("setVolume", from).isEmpty());
            QVERIFY(!fake.waitForMethod("setBrightness", from).isEmpty());
        }

        void aSupervisorOfAnotherProtocolIsNotTriedAgain()
        {
            FakeSupervisor fake;
            fake.answer_hello = false;
            SupervisorClient client(fake.endpoint(), "2.0.0");
            client.start(300ms);
            QJsonObject hello = fake.waitForMethod("hello");
            fake.send({{"jsonrpc", "2.0"}, {"id", hello.value("id")},
                       {"error", QJsonObject{{"code", -32002}, {"message", "protocol not supported"}}}});
            QTest::qWait(6000);
            QCOMPARE(fake.connections, 1);
            QVERIFY(!client.ready());
        }
};

QObject *createTestSupervisorClient()
{
    return new TestSupervisorClient;
}

#include "test_supervisor_client.moc"
