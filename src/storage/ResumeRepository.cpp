#include "storage/ResumeRepository.h"
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>
#include <utility>

ResumeRepository::ResumeRepository(QString databasePath)
    : databasePath_(std::move(databasePath)),
      connectionName_(QUuid::createUuid().toString(QUuid::WithoutBraces)) {}

ResumeRepository::~ResumeRepository() {
    if (db_.isValid()) {
        db_.close();
        db_ = QSqlDatabase();
        QSqlDatabase::removeDatabase(connectionName_);
    }
}

bool ResumeRepository::fail(QString* error, const QString& message) {
    if (error) *error = message;
    return false;
}

bool ResumeRepository::open(QString* error) {
    if (db_.isValid() && db_.isOpen()) return true;
    db_ = QSqlDatabase::addDatabase("QSQLITE", connectionName_);
    db_.setDatabaseName(databasePath_);
    if (!db_.open()) return fail(error, db_.lastError().text());
    QSqlQuery q(db_);
    if (!q.exec("PRAGMA foreign_keys = ON")) return fail(error, q.lastError().text());
    if (!q.exec("CREATE TABLE IF NOT EXISTS resumes (id INTEGER PRIMARY KEY, title TEXT NOT NULL)"))
        return fail(error, q.lastError().text());
    if (!q.exec("CREATE TABLE IF NOT EXISTS career_stages ("
                "id INTEGER PRIMARY KEY, resume_id INTEGER NOT NULL, position TEXT NOT NULL, "
                "company TEXT, period TEXT, description TEXT, "
                "FOREIGN KEY(resume_id) REFERENCES resumes(id) ON DELETE CASCADE)"))
        return fail(error, q.lastError().text());
    return true;
}

bool ResumeRepository::save(Resume& resume, QString* error) {
    if (!db_.isOpen()) return fail(error, "Database is not open");
    if (!db_.transaction()) return fail(error, db_.lastError().text());
    auto rollback = [&](const QString& message) { db_.rollback(); return fail(error, message); };
    QSqlQuery q(db_);
    if (resume.id == 0) {
        q.prepare("INSERT INTO resumes(title) VALUES(?)");
        q.addBindValue(resume.title);
        if (!q.exec()) return rollback(q.lastError().text());
        resume.id = q.lastInsertId().toLongLong();
    } else {
        q.prepare("UPDATE resumes SET title = ? WHERE id = ?");
        q.addBindValue(resume.title);
        q.addBindValue(resume.id);
        if (!q.exec()) return rollback(q.lastError().text());
        if (q.numRowsAffected() != 1) return rollback("Resume not found");
        q.prepare("DELETE FROM career_stages WHERE resume_id = ?");
        q.addBindValue(resume.id);
        if (!q.exec()) return rollback(q.lastError().text());
    }
    for (auto& stage : resume.stages) {
        q.prepare("INSERT INTO career_stages(resume_id,position,company,period,description) "
                  "VALUES(?,?,?,?,?)");
        q.addBindValue(resume.id);
        q.addBindValue(stage.position);
        q.addBindValue(stage.company);
        q.addBindValue(stage.period);
        q.addBindValue(stage.description);
        if (!q.exec()) return rollback(q.lastError().text());
        stage.id = q.lastInsertId().toLongLong();
    }
    if (!db_.commit()) return rollback(db_.lastError().text());
    return true;
}

std::optional<Resume> ResumeRepository::load(qint64 id, QString* error) const {
    if (!db_.isOpen()) { fail(error, "Database is not open"); return std::nullopt; }
    QSqlQuery q(db_);
    q.prepare("SELECT id,title FROM resumes WHERE id = ?");
    q.addBindValue(id);
    if (!q.exec()) { fail(error, q.lastError().text()); return std::nullopt; }
    if (!q.next()) { fail(error, "Resume not found"); return std::nullopt; }
    Resume resume;
    resume.id = q.value(0).toLongLong();
    resume.title = q.value(1).toString();
    q.prepare("SELECT id,position,company,period,description FROM career_stages "
              "WHERE resume_id = ? ORDER BY id");
    q.addBindValue(id);
    if (!q.exec()) { fail(error, q.lastError().text()); return std::nullopt; }
    while (q.next()) {
        resume.stages.push_back({q.value(0).toLongLong(), q.value(1).toString(),
                                 q.value(2).toString(), q.value(3).toString(),
                                 q.value(4).toString()});
    }
    return resume;
}
