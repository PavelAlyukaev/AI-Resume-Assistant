#include "web/WebServer.h"
#include "application/ResumeJson.h"
#include <QFile>
#include <QHostAddress>
#include <QJsonDocument>
#include <QTcpServer>
#include <utility>

namespace {

using Status = QHttpServerResponder::StatusCode;
constexpr qsizetype kMaxBodyBytes = 1024 * 1024;

void addSecurityHeaders(QHttpServerResponse& response) {
    QHttpHeaders headers = response.headers();
    headers.append("Cache-Control", "no-store");
    headers.append("X-Content-Type-Options", "nosniff");
    headers.append("Referrer-Policy", "no-referrer");
    headers.append("Content-Security-Policy",
                   "default-src 'none'; script-src 'self' 'unsafe-inline'; "
                   "style-src 'self' 'unsafe-inline'; connect-src 'self'; base-uri 'none'; "
                   "form-action 'none'; frame-ancestors 'none'");
    response.setHeaders(std::move(headers));
}

} // namespace

WebServer::WebServer(ResumeRepository& repository, QObject* parent)
    : QObject(parent), repository_(repository) {
    http_.route("/", QHttpServerRequest::Method::Get,
                [this](const QHttpServerRequest& request) { return handleIndex(request); });
    http_.route("/api/resumes/<arg>", QHttpServerRequest::Method::Get,
                [this](qint64 id, const QHttpServerRequest& request) { return handleLoad(id, request); });
    http_.route("/api/resumes", QHttpServerRequest::Method::Post,
                [this](const QHttpServerRequest& request) { return handleSave(0, request); });
    http_.route("/api/resumes/<arg>", QHttpServerRequest::Method::Put,
                [this](qint64 id, const QHttpServerRequest& request) { return handleSave(id, request); });
}

bool WebServer::listen(quint16 port, QString* error) {
    if (tcp_)
        return true;
    auto* tcp = new QTcpServer(this);
    if (!tcp->listen(QHostAddress::LocalHost, port) || !http_.bind(tcp)) {
        if (error)
            *error = tcp->errorString();
        delete tcp;
        return false;
    }
    tcp_ = tcp;
    return true;
}

quint16 WebServer::port() const { return tcp_ ? tcp_->serverPort() : 0; }

QUrl WebServer::url() const { return QUrl(QString("http://127.0.0.1:%1/").arg(port())); }

QHttpServerResponse WebServer::jsonReply(const QJsonObject& body, Status status) {
    QHttpServerResponse response("application/json; charset=utf-8",
                                 QJsonDocument(body).toJson(QJsonDocument::Compact), status);
    addSecurityHeaders(response);
    return response;
}

QHttpServerResponse WebServer::errorReply(const QString& message, Status status) {
    return jsonReply(QJsonObject{{"error", message}}, status);
}

// Defends the loopback API against DNS rebinding (Host check) and cross-site
// requests from other pages in the user's browser (Origin and JSON content type).
bool WebServer::isTrustedRequest(const QHttpServerRequest& request, bool requireJson,
                                 QHttpServerResponse* rejection) const {
    const QByteArray expectedPort = QByteArray::number(port());
    const QByteArray host = request.value("Host");
    const bool hostOk = host == "127.0.0.1:" + expectedPort || host == "localhost:" + expectedPort;
    if (!hostOk) {
        *rejection = errorReply("Forbidden host", Status::Forbidden);
        return false;
    }
    if (!requireJson)
        return true;
    const QByteArray origin = request.value("Origin");
    if (!origin.isEmpty() && origin != "http://" + host) {
        *rejection = errorReply("Forbidden origin", Status::Forbidden);
        return false;
    }
    if (!request.value("Content-Type").startsWith("application/json")) {
        *rejection = errorReply("Content-Type must be application/json", Status::UnsupportedMediaType);
        return false;
    }
    return true;
}

QHttpServerResponse WebServer::handleIndex(const QHttpServerRequest& request) const {
    QHttpServerResponse rejection = errorReply({}, Status::Forbidden);
    if (!isTrustedRequest(request, false, &rejection))
        return rejection;
    QFile file(":/web/index.html");
    if (!file.open(QIODevice::ReadOnly))
        return errorReply("UI is unavailable", Status::InternalServerError);
    QHttpServerResponse response("text/html; charset=utf-8", file.readAll(), Status::Ok);
    addSecurityHeaders(response);
    return response;
}

QHttpServerResponse WebServer::handleLoad(qint64 id, const QHttpServerRequest& request) const {
    QHttpServerResponse rejection = errorReply({}, Status::Forbidden);
    if (!isTrustedRequest(request, false, &rejection))
        return rejection;
    QString error;
    const auto resume = repository_.load(id, &error);
    if (!resume) {
        if (error == "Resume not found")
            return errorReply("Resume not found", Status::NotFound);
        return errorReply("Storage error", Status::InternalServerError);
    }
    return jsonReply(ResumeJson::toJson(*resume), Status::Ok);
}

QHttpServerResponse WebServer::handleSave(qint64 id, const QHttpServerRequest& request) const {
    QHttpServerResponse rejection = errorReply({}, Status::Forbidden);
    if (!isTrustedRequest(request, true, &rejection))
        return rejection;
    if (request.body().size() > kMaxBodyBytes)
        return errorReply("Request body is too large", Status::PayloadTooLarge);
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(request.body(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
        return errorReply("Body must be a JSON object", Status::BadRequest);
    QString error;
    auto resume = ResumeJson::fromJson(document.object(), &error);
    if (!resume)
        return errorReply(error, Status::BadRequest);
    resume->id = id;
    if (!repository_.save(*resume, &error)) {
        if (error == "Resume not found")
            return errorReply("Resume not found", Status::NotFound);
        return errorReply("Storage error", Status::InternalServerError);
    }
    return jsonReply(ResumeJson::toJson(*resume), id == 0 ? Status::Created : Status::Ok);
}
