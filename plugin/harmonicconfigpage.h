#ifndef HARMONICCONFIGPAGE_H
#define HARMONICCONFIGPAGE_H

#include <KTextEditor/ConfigPage>

class QComboBox;
class QLineEdit;
class QCheckBox;

class HarmonicConfigPage : public KTextEditor::ConfigPage {
    Q_OBJECT

  public:
    explicit HarmonicConfigPage(QWidget *parent = nullptr);
    ~HarmonicConfigPage() override;

    QString name() const override;
    QString fullName() const override;
    QIcon icon() const override;

  public Q_SLOTS:
    void apply() override;
    void reset() override;
    void defaults() override;

  private:
    QComboBox *m_backendCombo;
    QLineEdit *m_commandEdit;
    QLineEdit *m_apiKeyEdit;
    QCheckBox *m_contextCheck;
    QLineEdit *m_modelEdit;
};

#endif // HARMONICCONFIGPAGE_H
