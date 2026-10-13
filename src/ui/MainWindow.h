#pragma once
#include "domain/Resume.h"
#include "storage/ResumeRepository.h"
#include "web/WebServer.h"
#include <QMainWindow>
#include <memory>
class QLineEdit;
class QListWidget;
class QTextEdit;

class MainWindow final : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
private:
    void newResume();
    void addStage();
    void saveResume();
    void openResume();
    void openInBrowser();
    void selectStage(int row);
    void updateSelectedStage();
    void refreshList();
    void reportError(const QString& message);
    Resume resume_;
    std::unique_ptr<ResumeRepository> repository_;
    std::unique_ptr<WebServer> webServer_;
    QLineEdit* title_ = nullptr;
    QListWidget* stages_ = nullptr;
    QLineEdit* position_ = nullptr;
    QLineEdit* company_ = nullptr;
    QLineEdit* period_ = nullptr;
    QTextEdit* description_ = nullptr;
};
