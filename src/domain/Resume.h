#pragma once
#include <QString>
#include <QVector>

struct CareerStage {
    qint64 id = 0;
    QString position;
    QString company;
    QString period;
    QString description;
};

struct Resume {
    qint64 id = 0;
    QString title;
    QVector<CareerStage> stages;
};
