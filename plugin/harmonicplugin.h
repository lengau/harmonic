#ifndef HARMONICPLUGIN_H
#define HARMONICPLUGIN_H

#include <KTextEditor/Plugin>
#include <KTextEditor/MainWindow>
#include <KXMLGUIClient>

class HarmonicChatWidget;

class HarmonicPlugin : public KTextEditor::Plugin
{
    Q_OBJECT

public:
    explicit HarmonicPlugin(QObject *parent, const QVariantList &args);
    ~HarmonicPlugin() override;

    QObject *createView(KTextEditor::MainWindow *mainWindow) override;
    int configPages() const override;
    KTextEditor::ConfigPage *configPage(int number, QWidget *parent) override;
};

class HarmonicView : public QObject, public KXMLGUIClient
{
    Q_OBJECT

public:
    explicit HarmonicView(HarmonicPlugin *plugin, KTextEditor::MainWindow *mainWindow);
    ~HarmonicView() override;

private Q_SLOTS:
    void vibecode();
    void updateChatContext();

private:
    KTextEditor::MainWindow *m_mainWindow;
    HarmonicPlugin *m_plugin;
    QWidget *m_toolView;
    HarmonicChatWidget *m_chatWidget;
};

#endif // HARMONICPLUGIN_H
