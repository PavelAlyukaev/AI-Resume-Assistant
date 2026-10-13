#include "storage/ResumeRepository.h"
#include "web/WebServer.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QtTest>
#include <memory>

namespace {

struct Response {
    int status = 0;
    QByteArray headers;
    QByteArray body;
    QJsonObject json() const { return QJsonDocument::fromJson(body).object(); }
};

// Raw HTTP/1.1 client so the tests can control Host and Origin exactly.
Response send(quint16 port, const QByteArray& method, const QByteArray& path, const QByteArray& body = {},
              const QByteArray& host = {}, const QByteArray& origin = {},
              const QByteArray& contentType = "application/json") {
    QTcpSocket socket;
    socket.connectToHost(QHostAddress::LocalHost, port);
    if (!socket.waitForConnected(3000))
        return {};
    const QByteArray hostValue = host.isEmpty() ? "127.0.0.1:" + QByteArray::number(port) : host;
    QByteArray request = method + " " + path + " HTTP/1.1\r\nHost: " + hostValue + "\r\n";
    if (!origin.isEmpty())
        request += "Origin: " + origin + "\r\n";
    if (!body.isEmpty())
        request += "Content-Type: " + contentType + "\r\nContent-Length: " + QByteArray::number(body.size()) +
                   "\r\n";
    request += "Connection: close\r\n\r\n" + body;
    socket.write(request);
    QByteArray raw;
    QSignalSpy closed(&socket, &QTcpSocket::disconnected);
    QObject::connect(&socket, &QTcpSocket::readyRead, [&] { raw += socket.readAll(); });
    for (int i = 0; i < 50 && socket.state() != QAbstractSocket::UnconnectedState; ++i)
        closed.wait(100);
    raw += socket.readAll();
    Response response;
    const qsizetype split = raw.indexOf("\r\n\r\n");
    if (split < 0)
        return response;
    response.headers = raw.left(split);
    response.body = raw.mid(split + 4);
    const QList<QByteArray> statusLine = response.headers.left(response.headers.indexOf("\r\n")).split(' ');
    if (statusLine.size() >= 2)
        response.status = statusLine[1].toInt();
    return response;
}

QByteArray resumeBody(const QString& title, const QString& position) {
    return QJsonDocument(QJsonObject{{"title", title},
                                     {"stages", QJsonArray{QJsonObject{{"position", position},
                                                                       {"company", "Тестовая компания"},
                                                                       {"period", "2020-2023"},
                                                                       {"description", "Разработка"}}}}})
        .toJson(QJsonDocument::Compact);
}

} // namespace

class WebApiTest : public QObject {
    Q_OBJECT
  private slots:
    void init() {
        dir_ = std::make_unique<QTemporaryDir>();
        QVERIFY(dir_->isValid());
        repo_ = std::make_unique<ResumeRepository>(dir_->filePath("web.sqlite"));
        QString error;
        QVERIFY2(repo_->open(&error), qPrintable(error));
        server_ = std::make_unique<WebServer>(*repo_);
        QVERIFY2(server_->listen(0, &error), qPrintable(error));
        QVERIFY(server_->port() != 0);
    }

    void cleanup() {
        server_.reset();
        repo_.reset();
        dir_.reset();
    }

    void listensOnLoopbackOnly() { QCOMPARE(server_->url().host(), QString("127.0.0.1")); }

    void servesIndexWithSecurityHeaders() {
        const Response r = send(server_->port(), "GET", "/");
        QCOMPARE(r.status, 200);
        QVERIFY(r.body.contains("<!doctype html>"));
        QVERIFY(r.headers.toLower().contains("content-security-policy: default-src 'none'"));
        QVERIFY(r.headers.toLower().contains("x-content-type-options: nosniff"));
    }

    void createLoadUpdateRoundTrip() {
        const quint16 port = server_->port();
        const Response created = send(port, "POST", "/api/resumes", resumeBody("Резюме", "Инженер"));
        QCOMPARE(created.status, 201);
        const qint64 id = created.json().value("id").toInteger();
        QVERIFY(id > 0);

        const Response loaded = send(port, "GET", "/api/resumes/" + QByteArray::number(id));
        QCOMPARE(loaded.status, 200);
        QCOMPARE(loaded.json().value("title").toString(), QString("Резюме"));
        const QJsonArray stages = loaded.json().value("stages").toArray();
        QCOMPARE(stages.size(), 1);
        QCOMPARE(stages[0].toObject().value("position").toString(), QString("Инженер"));

        const Response updated = send(port, "PUT", "/api/resumes/" + QByteArray::number(id),
                                      resumeBody("Резюме 2", "Старший инженер"));
        QCOMPARE(updated.status, 200);

        // The data is visible to the Qt UI side through the shared repository.
        QString error;
        const auto stored = repo_->load(id, &error);
        QVERIFY2(stored.has_value(), qPrintable(error));
        QCOMPARE(stored->title, QString("Резюме 2"));
        QCOMPARE(stored->stages.size(), 1);
        QCOMPARE(stored->stages[0].position, QString("Старший инженер"));
    }

    void unknownResumeIs404() {
        QCOMPARE(send(server_->port(), "GET", "/api/resumes/999").status, 404);
        QCOMPARE(send(server_->port(), "PUT", "/api/resumes/999", resumeBody("T", "P")).status, 404);
    }

    void invalidBodiesAre400() {
        const quint16 port = server_->port();
        QCOMPARE(send(port, "POST", "/api/resumes", "not json").status, 400);
        QCOMPARE(send(port, "POST", "/api/resumes", "[1,2]").status, 400);
        QCOMPARE(send(port, "POST", "/api/resumes", R"({"title":"  "})").status, 400);
    }

    void rejectsForeignHost() {
        const Response r = send(server_->port(), "GET", "/api/resumes/1", {}, "evil.example:80");
        QCOMPARE(r.status, 403);
        QCOMPARE(send(server_->port(), "GET", "/", {}, "evil.example:80").status, 403);
    }

    void rejectsCrossOriginWrite() {
        const Response r =
            send(server_->port(), "POST", "/api/resumes", resumeBody("T", "P"), {}, "http://evil.example");
        QCOMPARE(r.status, 403);
    }

    void rejectsNonJsonContentType() {
        const Response r =
            send(server_->port(), "POST", "/api/resumes", resumeBody("T", "P"), {}, {}, "text/plain");
        QCOMPARE(r.status, 415);
    }

    void errorsDoNotLeakStorageDetails() {
        const Response r = send(server_->port(), "GET", "/api/resumes/999");
        QVERIFY(!r.body.contains("sqlite"));
        QVERIFY(!r.body.contains(".sqlite"));
    }

  private:
    std::unique_ptr<QTemporaryDir> dir_;
    std::unique_ptr<ResumeRepository> repo_;
    std::unique_ptr<WebServer> server_;
};
QTEST_GUILESS_MAIN(WebApiTest)
#include "WebApiTest.moc"
