#ifndef APP_UI_PLUGIN_H
#define APP_UI_PLUGIN_H

#include <QString>
#include <QVariantList>
#include "app_ui_interface.h"
#include "LogosViewPluginBase.h"
#include "rep_app_ui_source.h"
#include "logos_sdk.h"

class LogosAPI;

class AppUiPlugin : public AppUiSimpleSource,
                    public AppUiInterface,
                    public AppUiViewPluginBase
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID AppUiInterface_iid FILE "metadata.json")
    Q_INTERFACES(AppUiInterface)

public:
    explicit AppUiPlugin(QObject* parent = nullptr);
    ~AppUiPlugin() override;

    QString name()    const override { return "app_ui"; }
    QString version() const override { return "0.1.0"; }

    Q_INVOKABLE void initLogos(LogosAPI* api);

signals:
    void eventResponse(const QString& eventName, const QVariantList& args);

private slots:
    void refresh();

private:
    LogosAPI* m_logosAPI = nullptr;
    LogosModules* m_logos = nullptr;
};

#endif
