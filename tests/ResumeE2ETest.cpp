#include "domain/Resume.h"
#include "storage/ResumeRepository.h"
#include <QTemporaryDir>
#include <QtTest>

class ResumeE2ETest : public QObject {
    Q_OBJECT
private slots:
    void createSaveReopenAndUpdate() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("test.sqlite");
        qint64 savedId = 0;
        {
            ResumeRepository repo(path);
            QString error;
            QVERIFY2(repo.open(&error), qPrintable(error));
            Resume resume;
            resume.title = "Тестовое резюме";
            resume.stages.push_back({0, "Инженер", "Тестовая компания", "2020-2023", "Разработка"});
            QVERIFY2(repo.save(resume, &error), qPrintable(error));
            QVERIFY(resume.id > 0);
            savedId = resume.id;
        }
        {
            ResumeRepository repo(path);
            QString error;
            QVERIFY2(repo.open(&error), qPrintable(error));
            auto loaded = repo.load(savedId, &error);
            QVERIFY2(loaded.has_value(), qPrintable(error));
            QCOMPARE(loaded->title, QString("Тестовое резюме"));
            QCOMPARE(loaded->stages.size(), 1);
            QCOMPARE(loaded->stages[0].position, QString("Инженер"));
            loaded->stages[0].description = "Разработка и тестирование";
            QVERIFY2(repo.save(*loaded, &error), qPrintable(error));
            auto updated = repo.load(savedId, &error);
            QVERIFY(updated.has_value());
            QCOMPARE(updated->stages.size(), 1);
            QCOMPARE(updated->stages[0].description, QString("Разработка и тестирование"));
        }
    }
};
QTEST_GUILESS_MAIN(ResumeE2ETest)
#include "ResumeE2ETest.moc"
