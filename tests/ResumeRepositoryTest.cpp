#include "storage/ResumeRepository.h"
#include "domain/Resume.h"
#include <QTemporaryDir>
#include <QtTest>

class ResumeRepositoryTest : public QObject {
    Q_OBJECT
  private slots:
    void saveBeforeOpenFails() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        ResumeRepository repo(dir.filePath("test.sqlite"));
        Resume resume;
        resume.title = "Резюме";
        QString error;
        QVERIFY(!repo.save(resume, &error));
        QCOMPARE(error, QString("Database is not open"));
        QCOMPARE(resume.id, qint64(0));
    }

    void loadBeforeOpenFails() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        ResumeRepository repo(dir.filePath("test.sqlite"));
        QString error;
        QVERIFY(!repo.load(1, &error).has_value());
        QCOMPARE(error, QString("Database is not open"));
    }

    void loadUnknownIdFails() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        ResumeRepository repo(dir.filePath("test.sqlite"));
        QString error;
        QVERIFY2(repo.open(&error), qPrintable(error));
        QVERIFY(!repo.load(42, &error).has_value());
        QCOMPARE(error, QString("Resume not found"));
    }

    void updateUnknownIdRollsBack() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("test.sqlite");
        ResumeRepository repo(path);
        QString error;
        QVERIFY2(repo.open(&error), qPrintable(error));
        Resume ghost;
        ghost.id = 99;
        ghost.title = "Призрак";
        ghost.stages.push_back({0, "Инженер", "Компания", "2020", "Описание"});
        QVERIFY(!repo.save(ghost, &error));
        QCOMPARE(error, QString("Resume not found"));
        QCOMPARE(ghost.stages[0].id, qint64(0));
        // The failed UPDATE must not leave orphan stages behind.
        QVERIFY(!repo.load(99, &error).has_value());
        QCOMPARE(error, QString("Resume not found"));
    }

    void invalidTitleIsRejectedAndNotPersisted() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        ResumeRepository repo(dir.filePath("test.sqlite"));
        QString error;
        QVERIFY2(repo.open(&error), qPrintable(error));
        Resume resume; // title is NOT NULL in the schema; a null QString is rejected
        resume.stages.push_back({0, "Инженер", "Компания", "2020", "Описание"});
        QVERIFY(!repo.save(resume, &error));
        QVERIFY(!error.isEmpty());
        QCOMPARE(resume.id, qint64(0));
    }

    void saveAssignsIdsAndKeepsStageOrder() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("test.sqlite");
        qint64 id = 0;
        {
            ResumeRepository repo(path);
            QString error;
            QVERIFY2(repo.open(&error), qPrintable(error));
            Resume resume;
            resume.title = "Порядок";
            resume.stages.push_back({0, "Первый", "А", "2018", ""});
            resume.stages.push_back({0, "Второй", "Б", "2019", ""});
            resume.stages.push_back({0, "Третий", "В", "2020", ""});
            QVERIFY2(repo.save(resume, &error), qPrintable(error));
            QVERIFY(resume.id > 0);
            QVERIFY(resume.stages[0].id > 0);
            QVERIFY(resume.stages[0].id < resume.stages[1].id);
            QVERIFY(resume.stages[1].id < resume.stages[2].id);
            id = resume.id;
        }
        ResumeRepository repo(path);
        QString error;
        QVERIFY2(repo.open(&error), qPrintable(error));
        const auto loaded = repo.load(id, &error);
        QVERIFY2(loaded.has_value(), qPrintable(error));
        QCOMPARE(loaded->stages.size(), 3);
        QCOMPARE(loaded->stages[0].position, QString("Первый"));
        QCOMPARE(loaded->stages[1].position, QString("Второй"));
        QCOMPARE(loaded->stages[2].position, QString("Третий"));
    }

    void updateReplacesStagesInsteadOfAppending() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("test.sqlite");
        qint64 id = 0;
        {
            ResumeRepository repo(path);
            QString error;
            QVERIFY2(repo.open(&error), qPrintable(error));
            Resume resume;
            resume.title = "Замена";
            resume.stages.push_back({0, "Старый", "А", "2018", ""});
            resume.stages.push_back({0, "Ещё старый", "Б", "2019", ""});
            QVERIFY2(repo.save(resume, &error), qPrintable(error));
            id = resume.id;

            resume.title = "Замена v2";
            resume.stages.removeFirst();
            resume.stages[0].position = "Новый";
            QVERIFY2(repo.save(resume, &error), qPrintable(error));
        }
        ResumeRepository repo(path);
        QString error;
        QVERIFY2(repo.open(&error), qPrintable(error));
        const auto loaded = repo.load(id, &error);
        QVERIFY2(loaded.has_value(), qPrintable(error));
        QCOMPARE(loaded->title, QString("Замена v2"));
        QCOMPARE(loaded->stages.size(), 1);
        QCOMPARE(loaded->stages[0].position, QString("Новый"));
    }

    void emptyStagesAndEmptyFieldsPersist() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("test.sqlite");
        qint64 id = 0;
        {
            ResumeRepository repo(path);
            QString error;
            QVERIFY2(repo.open(&error), qPrintable(error));
            Resume resume;
            resume.title = "Пустое";
            QVERIFY2(repo.save(resume, &error), qPrintable(error));
            id = resume.id;
        }
        ResumeRepository repo(path);
        QString error;
        QVERIFY2(repo.open(&error), qPrintable(error));
        const auto loaded = repo.load(id, &error);
        QVERIFY2(loaded.has_value(), qPrintable(error));
        QCOMPARE(loaded->title, QString("Пустое"));
        QVERIFY(loaded->stages.isEmpty());
    }

    void titleWithSqlMetacharactersIsStoredLiterally() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("test.sqlite");
        const QString tricky = "Robert'); DROP TABLE resumes;--";
        qint64 id = 0;
        {
            ResumeRepository repo(path);
            QString error;
            QVERIFY2(repo.open(&error), qPrintable(error));
            Resume resume;
            resume.title = tricky;
            QVERIFY2(repo.save(resume, &error), qPrintable(error));
            id = resume.id;
        }
        ResumeRepository repo(path);
        QString error;
        QVERIFY2(repo.open(&error), qPrintable(error));
        const auto loaded = repo.load(id, &error);
        QVERIFY2(loaded.has_value(), qPrintable(error));
        QCOMPARE(loaded->title, tricky);
    }

    void separateResumesDoNotShareStages() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        ResumeRepository repo(dir.filePath("test.sqlite"));
        QString error;
        QVERIFY2(repo.open(&error), qPrintable(error));
        Resume first;
        first.title = "Первое";
        first.stages.push_back({0, "Только в первом", "", "", ""});
        QVERIFY2(repo.save(first, &error), qPrintable(error));
        Resume second;
        second.title = "Второе";
        QVERIFY2(repo.save(second, &error), qPrintable(error));
        QVERIFY(second.id != first.id);

        const auto loadedSecond = repo.load(second.id, &error);
        QVERIFY2(loadedSecond.has_value(), qPrintable(error));
        QVERIFY(loadedSecond->stages.isEmpty());
        const auto loadedFirst = repo.load(first.id, &error);
        QVERIFY2(loadedFirst.has_value(), qPrintable(error));
        QCOMPARE(loadedFirst->stages.size(), 1);
    }
};
QTEST_GUILESS_MAIN(ResumeRepositoryTest)
#include "ResumeRepositoryTest.moc"
