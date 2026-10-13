#include "application/ResumeJson.h"
#include <QJsonArray>
#include <QtTest>

class ResumeJsonTest : public QObject {
    Q_OBJECT
  private slots:
    void roundTrip() {
        Resume resume;
        resume.id = 7;
        resume.title = "Тестовое резюме";
        resume.stages.push_back({3, "Инженер", "Тестовая компания", "2020-2023", "Разработка"});
        const QJsonObject json = ResumeJson::toJson(resume);
        QCOMPARE(json.value("id").toInteger(), 7);
        QString error;
        const auto parsed = ResumeJson::fromJson(json, &error);
        QVERIFY2(parsed.has_value(), qPrintable(error));
        QCOMPARE(parsed->id, qint64(0)); // the id is never taken from the body
        QCOMPARE(parsed->title, resume.title);
        QCOMPARE(parsed->stages.size(), 1);
        QCOMPARE(parsed->stages[0].position, QString("Инженер"));
        QCOMPARE(parsed->stages[0].company, QString("Тестовая компания"));
        QCOMPARE(parsed->stages[0].period, QString("2020-2023"));
        QCOMPARE(parsed->stages[0].description, QString("Разработка"));
    }

    void missingStagesMeansEmptyList() {
        const auto parsed = ResumeJson::fromJson(QJsonObject{{"title", "Резюме"}});
        QVERIFY(parsed.has_value());
        QVERIFY(parsed->stages.isEmpty());
    }

    void optionalStageFieldsDefaultToEmpty() {
        const QJsonObject json{{"title", "Резюме"},
                               {"stages", QJsonArray{QJsonObject{{"position", "Аналитик"}}}}};
        const auto parsed = ResumeJson::fromJson(json);
        QVERIFY(parsed.has_value());
        QCOMPARE(parsed->stages[0].company, QString());
        QCOMPARE(parsed->stages[0].description, QString());
    }

    void rejectsInvalidInput_data() {
        QTest::addColumn<QJsonObject>("json");
        const auto stage = [](const QJsonObject& o) {
            return QJsonObject{{"title", "T"}, {"stages", QJsonArray{o}}};
        };
        QTest::newRow("no title") << QJsonObject{};
        QTest::newRow("blank title") << QJsonObject{{"title", "   "}};
        QTest::newRow("title not string") << QJsonObject{{"title", 5}};
        QTest::newRow("title too long") << QJsonObject{{"title", QString(201, 'a')}};
        QTest::newRow("stages not array") << QJsonObject{{"title", "T"}, {"stages", "x"}};
        QTest::newRow("stage not object") << QJsonObject{{"title", "T"}, {"stages", QJsonArray{1}}};
        QTest::newRow("stage without position") << stage(QJsonObject{{"company", "C"}});
        QTest::newRow("position not string") << stage(QJsonObject{{"position", 1}});
        QTest::newRow("description too long")
            << stage(QJsonObject{{"position", "P"}, {"description", QString(10001, 'a')}});
    }

    void rejectsInvalidInput() {
        QFETCH(QJsonObject, json);
        QString error;
        QVERIFY(!ResumeJson::fromJson(json, &error).has_value());
        QVERIFY(!error.isEmpty());
    }

    void rejectsTooManyStages() {
        QJsonArray stages;
        for (int i = 0; i < 201; ++i)
            stages.append(QJsonObject{{"position", "P"}});
        QVERIFY(!ResumeJson::fromJson(QJsonObject{{"title", "T"}, {"stages", stages}}).has_value());
    }
};
QTEST_GUILESS_MAIN(ResumeJsonTest)
#include "ResumeJsonTest.moc"
