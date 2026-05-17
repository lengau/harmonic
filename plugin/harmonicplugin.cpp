#include "harmonicplugin.h"
#include "harmonicconfigpage.h"

#include <KPluginFactory>

K_PLUGIN_FACTORY_WITH_JSON(HarmonicPluginFactory, "harmonicplugin.json",
                           registerPlugin<HarmonicPlugin>();)

HarmonicPlugin::HarmonicPlugin(QObject *parent, const QVariantList &args)
    : KTextEditor::Plugin(parent)
{
    Q_UNUSED(args);
}

HarmonicPlugin::~HarmonicPlugin() = default;

QObject *HarmonicPlugin::createView(KTextEditor::MainWindow *mainWindow)
{
    return new HarmonicView(this, mainWindow);
}

int HarmonicPlugin::configPages() const
{
    return 1;
}

KTextEditor::ConfigPage *HarmonicPlugin::configPage(int number, QWidget *parent)
{
    if (number == 0) {
        return new HarmonicConfigPage(parent);
    }
    return nullptr;
}

#include "harmonicplugin.moc"
