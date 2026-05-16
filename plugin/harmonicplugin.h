#ifndef HARMONICPLUGIN_H
#define HARMONICPLUGIN_H

#include <KTextEditor/Cursor>
#include <KTextEditor/MainWindow>
#include <KTextEditor/Plugin>
#include <KXMLGUIClient>

#include <QFutureWatcher>
#include <QPointer>
#include <QString>

class QAction;
class HarmonicChatWidget;
namespace QKeychain {
class ReadPasswordJob;
}
namespace KTextEditor {
class Document;
}

class HarmonicPlugin : public KTextEditor::Plugin {
  Q_OBJECT

public:
  explicit HarmonicPlugin(QObject *parent, const QVariantList &args);
  ~HarmonicPlugin() override;

  QObject *createView(KTextEditor::MainWindow *mainWindow) override;
  int configPages() const override;
  KTextEditor::ConfigPage *configPage(int number, QWidget *parent) override;
};

class HarmonicView : public QObject, public KXMLGUIClient {
  Q_OBJECT

public:
  explicit HarmonicView(HarmonicPlugin *plugin,
                        KTextEditor::MainWindow *mainWindow);
  ~HarmonicView() override;

private Q_SLOTS:
  void vibecode();
  void updateChatContext();
  void handleVibecodeFinished();
  void onVibecodeApiKeyJobFinished();

private:
  void showStatusMessage(const QString &message, int timeoutMs = 0) const;
  void startVibecodeGeneration();

  KTextEditor::MainWindow *m_mainWindow;
  HarmonicPlugin *m_plugin;
  QWidget *m_toolView = nullptr;
  HarmonicChatWidget *m_chatWidget = nullptr;
  QAction *m_vibecodeAction = nullptr;
  QFutureWatcher<QString> *m_vibecodeWatcher = nullptr;
  QPointer<KTextEditor::Document> m_pendingDocument;
  int m_pendingInsertLine = -1;

  // Async API key reading for vibecode
  QKeychain::ReadPasswordJob *m_vibecodeApiKeyJob = nullptr;
  QString m_vibecodePrompt;
  QString m_vibecodeApiKey;
};

#endif // HARMONICPLUGIN_H
