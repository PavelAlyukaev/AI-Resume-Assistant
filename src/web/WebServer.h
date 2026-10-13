#pragma once
#include "storage/ResumeRepository.h"
#include <QHttpServer>
#include <QObject>
#include <QString>
#include <QUrl>

class QTcpServer;

// Local-only HTTP front end: serves the HTML UI and a small JSON API over the
// same ResumeRepository as the Qt Widgets UI. Binds to 127.0.0.1 exclusively.
class WebServer final : public QObject {
    Q_OBJECT
  public:
    explicit WebServer(ResumeRepository& repository, QObject* parent = nullptr);
    // port 0 picks any free port.
    bool listen(quint16 port, QString* error = nullptr);
    quint16 port() const;
    QUrl url() const;

  private:
    QHttpServerResponse handleIndex(const QHttpServerRequest& request) const;
    QHttpServerResponse handleLoad(qint64 id, const QHttpServerRequest& request) const;
    QHttpServerResponse handleSave(qint64 id, const QHttpServerRequest& request) const;
    bool isTrustedRequest(const QHttpServerRequest& request, bool requireJson,
                          QHttpServerResponse* rejection) const;
    static QHttpServerResponse jsonReply(const QJsonObject& body, QHttpServerResponder::StatusCode status);
    static QHttpServerResponse errorReply(const QString& message, QHttpServerResponder::StatusCode status);

    ResumeRepository& repository_;
    QHttpServer http_;
    QTcpServer* tcp_ = nullptr;
};
