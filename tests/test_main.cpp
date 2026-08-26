#include <QApplication>

int runRpcTests(int argc, char *argv[]);
int runModelsTests(int argc, char *argv[]);
int runSettingsTests(int argc, char *argv[]);
int runI18nTests(int argc, char *argv[]);
int runThemeTests(int argc, char *argv[]);
int runPieceMapTests(int argc, char *argv[]);
int runI2pdTunnelsTests(int argc, char *argv[]);
int runTorrentCreateTests(int argc, char *argv[]);

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    int status = 0;
    status |= runRpcTests(argc, argv);
    status |= runModelsTests(argc, argv);
    status |= runSettingsTests(argc, argv);
    status |= runI18nTests(argc, argv);
    status |= runThemeTests(argc, argv);
    status |= runPieceMapTests(argc, argv);
    status |= runI2pdTunnelsTests(argc, argv);
    status |= runTorrentCreateTests(argc, argv);
    return status;
}
