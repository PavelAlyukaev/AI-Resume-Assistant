// Helper for the browser e2e test: serves the web UI on a temporary database.
// Prints the chosen port to stdout and runs until the process is terminated.
#include "storage/ResumeRepository.h"
#include "web/WebServer.h"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <cstdio>

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    QTemporaryDir dir;
    if (!dir.isValid())
        return 2;
    ResumeRepository repository(dir.filePath("e2e.sqlite"));
    QString error;
    if (!repository.open(&error))
        return 3;
    WebServer server(repository);
    if (!server.listen(0, &error))
        return 4;
    std::printf("PORT=%u\n", server.port());
    std::fflush(stdout);
    return app.exec();
}
