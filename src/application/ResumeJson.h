#pragma once
#include "domain/Resume.h"
#include <QJsonObject>
#include <QString>
#include <optional>

namespace ResumeJson {

QJsonObject toJson(const Resume& resume);
// Validates untrusted input; the resume id is never read from the body.
std::optional<Resume> fromJson(const QJsonObject& object, QString* error = nullptr);

} // namespace ResumeJson
