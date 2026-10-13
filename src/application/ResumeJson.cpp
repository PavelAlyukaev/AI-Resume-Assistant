#include "application/ResumeJson.h"
#include <QJsonArray>
#include <QJsonValue>

namespace {

constexpr int kMaxShortText = 200;
constexpr int kMaxDescription = 10000;
constexpr int kMaxStages = 200;

bool fail(QString* error, const QString& message) {
    if (error)
        *error = message;
    return false;
}

bool readText(const QJsonObject& object, const QString& key, int maxLength, bool required, QString& out,
              QString* error) {
    const QJsonValue value = object.value(key);
    if (value.isUndefined() || value.isNull()) {
        if (required)
            return fail(error, QString("Field '%1' is required").arg(key));
        out.clear();
        return true;
    }
    if (!value.isString())
        return fail(error, QString("Field '%1' must be a string").arg(key));
    out = value.toString();
    if (required && out.trimmed().isEmpty())
        return fail(error, QString("Field '%1' is empty").arg(key));
    if (out.size() > maxLength)
        return fail(error, QString("Field '%1' is too long").arg(key));
    return true;
}

} // namespace

namespace ResumeJson {

QJsonObject toJson(const Resume& resume) {
    QJsonArray stages;
    for (const auto& stage : resume.stages) {
        stages.append(QJsonObject{{"id", stage.id},
                                  {"position", stage.position},
                                  {"company", stage.company},
                                  {"period", stage.period},
                                  {"description", stage.description}});
    }
    return QJsonObject{{"id", resume.id}, {"title", resume.title}, {"stages", stages}};
}

std::optional<Resume> fromJson(const QJsonObject& object, QString* error) {
    Resume resume;
    if (!readText(object, "title", kMaxShortText, true, resume.title, error))
        return std::nullopt;
    const QJsonValue stages = object.value("stages");
    if (stages.isUndefined() || stages.isNull())
        return resume;
    if (!stages.isArray()) {
        fail(error, "Field 'stages' must be an array");
        return std::nullopt;
    }
    const QJsonArray array = stages.toArray();
    if (array.size() > kMaxStages) {
        fail(error, "Too many stages");
        return std::nullopt;
    }
    for (const QJsonValue& item : array) {
        if (!item.isObject()) {
            fail(error, "Each stage must be an object");
            return std::nullopt;
        }
        const QJsonObject stageObject = item.toObject();
        CareerStage stage;
        if (!readText(stageObject, "position", kMaxShortText, true, stage.position, error) ||
            !readText(stageObject, "company", kMaxShortText, false, stage.company, error) ||
            !readText(stageObject, "period", kMaxShortText, false, stage.period, error) ||
            !readText(stageObject, "description", kMaxDescription, false, stage.description, error))
            return std::nullopt;
        resume.stages.push_back(stage);
    }
    return resume;
}

} // namespace ResumeJson
