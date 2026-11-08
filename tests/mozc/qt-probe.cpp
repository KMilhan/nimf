#include <QApplication>
#include <QSaveFile>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QWidget>

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QWidget window;
    window.setWindowTitle("Nimf Mozc probe");
    window.resize(650, 220);
    QVBoxLayout layout(&window);
    QLineEdit first, second;
    layout.addWidget(&first);
    layout.addWidget(&second);
    auto save = [&] {
        QSaveFile file(qEnvironmentVariable("NIMF_TEST_STATE"));
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
            qFatal("Cannot write test state");
        int focused = first.hasFocus() ? 1 : second.hasFocus() ? 2 : 0;
        file.write((first.text() + "\n" + second.text() + "\n" + QString::number(focused) + "\n").toUtf8());
        if (!file.commit())
            qFatal("Cannot commit test state");
    };
    QObject::connect(&first, &QLineEdit::textChanged, save);
    QObject::connect(&second, &QLineEdit::textChanged, save);
    QObject::connect(&app, &QApplication::focusChanged, save);
    save();
    window.show();
    first.setFocus();
    return app.exec();
}
