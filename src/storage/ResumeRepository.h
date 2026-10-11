#pragma once
#include "domain/Resume.h"
#include <QSqlDatabase>
#include <QString>
#include <optional>

class ResumeRepository {
public:
    explicit ResumeRepository(QString databasePath);
    ~ResumeRepository();
    ResumeRepository(const ResumeRepository&) = delete;
    ResumeRepository& operator=(const ResumeRepository&) = delete;
    bool open(QString* error = nullptr);
    bool save(Resume& resume, QString* error = nullptr);
    std::optional<Resume> load(qint64 id, QString* error = nullptr) const;
private:
    QString databasePath_;
    QString connectionName_;
    QSqlDatabase db_;
    static bool fail(QString* error, const QString& message);
};
