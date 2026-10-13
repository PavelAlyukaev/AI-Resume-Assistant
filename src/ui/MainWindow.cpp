#include "ui/MainWindow.h"
#include <QAction>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QWidget>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setWindowTitle("AI Resume Assistant — Sprint 1");
    resize(950, 550);
    auto *central = new QWidget(this);
    auto *root = new QVBoxLayout(central);
    title_ = new QLineEdit(central);
    title_->setPlaceholderText("Название резюме");
    root->addWidget(new QLabel("Название резюме", central));
    root->addWidget(title_);
    auto *columns = new QHBoxLayout;
    stages_ = new QListWidget(central);
    columns->addWidget(stages_, 1);
    auto *form = new QFormLayout;
    position_ = new QLineEdit(central);
    company_ = new QLineEdit(central);
    period_ = new QLineEdit(central);
    description_ = new QTextEdit(central);
    form->addRow("Должность", position_);
    form->addRow("Компания", company_);
    form->addRow("Период", period_);
    form->addRow("Описание", description_);
    auto *editor = new QWidget(central);
    editor->setLayout(form);
    columns->addWidget(editor, 2);
    root->addLayout(columns);
    auto *buttons = new QHBoxLayout;
    auto *add = new QPushButton("Добавить этап", central);
    auto *apply = new QPushButton("Применить изменения", central);
    buttons->addWidget(add);
    buttons->addWidget(apply);
    root->addLayout(buttons);
    setCentralWidget(central);
    auto *file = menuBar()->addMenu("Файл");
    file->addAction("Новое", this, &MainWindow::newResume);
    file->addAction("Открыть по ID", this, &MainWindow::openResume);
    file->addAction("Сохранить", this, &MainWindow::saveResume);
    file->addSeparator();
    file->addAction("Открыть веб-версию", this, &MainWindow::openInBrowser);
    connect(add, &QPushButton::clicked, this, &MainWindow::addStage);
    connect(apply, &QPushButton::clicked, this, &MainWindow::updateSelectedStage);
    connect(stages_, &QListWidget::currentRowChanged, this, &MainWindow::selectStage);
    const auto dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dataDir);
    repository_ = std::make_unique<ResumeRepository>(dataDir + "/resumes.sqlite");
    QString error;

    if (!repository_->open(&error))
        reportError(error);
    webServer_ = std::make_unique<WebServer>(*repository_);
    constexpr quint16 kDefaultWebPort = 8765;
    if (!webServer_->listen(kDefaultWebPort, &error) && !webServer_->listen(0, &error)) {
        webServer_.reset();
        statusBar()->showMessage("Веб-версия недоступна", 8000);
    } else {
        statusBar()->showMessage("Веб-версия: " + webServer_->url().toString(), 8000);
    }

    newResume();
}

void MainWindow::newResume() {
    resume_ = Resume{};
    title_->setText("Моё резюме");
    refreshList();
    selectStage(-1);
}

void MainWindow::addStage() {
    resume_.stages.push_back({0, "Новая должность", "", "", ""});
    refreshList();
    stages_->setCurrentRow(resume_.stages.size() - 1);
}

void MainWindow::refreshList() {
    stages_->clear();
    for (const auto &stage : resume_.stages)
        stages_->addItem(stage.position);
}

void MainWindow::selectStage(int row) {
    const bool valid = row >= 0 && row < resume_.stages.size();
    position_->setEnabled(valid);
    company_->setEnabled(valid);
    period_->setEnabled(valid);
    description_->setEnabled(valid);
    const CareerStage stage = valid ? resume_.stages.at(row) : CareerStage{};
    position_->setText(stage.position);
    company_->setText(stage.company);
    period_->setText(stage.period);
    description_->setPlainText(stage.description);
}

void MainWindow::updateSelectedStage() {
    const int row = stages_->currentRow();
    if (row < 0 || row >= resume_.stages.size())
        return;
    auto &stage = resume_.stages[row];
    stage.position = position_->text();
    stage.company = company_->text();
    stage.period = period_->text();
    stage.description = description_->toPlainText();
    refreshList();
    stages_->setCurrentRow(row);
}

void MainWindow::saveResume() {
    updateSelectedStage();
    resume_.title = title_->text();
    QString error;
    if (!repository_->save(resume_, &error)) {
        reportError(error);
        return;
    }
    statusBar()->showMessage(QString("Сохранено. ID: %1").arg(resume_.id), 8000);
}

void MainWindow::openResume() {
    bool ok = false;
    const auto id = QInputDialog::getInt(this, "Открыть резюме", "ID резюме", 1, 1, 999999, 1, &ok);
    if (!ok)
        return;
    QString error;
    const auto loaded = repository_->load(id, &error);
    if (!loaded) {
        reportError(error);
        return;
    }
    resume_ = *loaded;
    title_->setText(resume_.title);
    refreshList();
    if (!resume_.stages.isEmpty())
        stages_->setCurrentRow(0);
    else
        selectStage(-1);
}


void MainWindow::openInBrowser() {
    if (!webServer_) { reportError("Веб-версия не запущена"); return; }
    QDesktopServices::openUrl(webServer_->url());
}

void MainWindow::reportError(const QString& message) {
    QMessageBox::critical(this, "Ошибка", message);
}

