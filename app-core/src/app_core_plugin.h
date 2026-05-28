#ifndef APP_CORE_PLUGIN_H
#define APP_CORE_PLUGIN_H

#include <QObject>
#include <QString>
#include <QVariantList>
#include "app_core_interface.h"
#include "logos_api.h"
#include "logos_sdk.h"

class AppCorePlugin : public QObject, public AppCoreInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID AppCoreInterface_iid FILE "metadata.json")
    Q_INTERFACES(AppCoreInterface PluginInterface)

public:
    explicit AppCorePlugin(QObject* parent = nullptr);
    ~AppCorePlugin() override;

    QString name() const override { return "app_core"; }
    QString version() const override { return "0.1.0"; }

    Q_INVOKABLE bool storageStarted() override;
    Q_INVOKABLE bool storageConnected() override;
    Q_INVOKABLE bool deliveryStarted() override;
    Q_INVOKABLE bool deliveryConnected() override;

    Q_INVOKABLE void initLogos(LogosAPI* logosAPIInstance);

signals:
    void eventResponse(const QString& eventName, const QVariantList& args);

private:
    LogosAPI* m_logosAPI = nullptr;
    LogosModules* m_logos = nullptr;
    bool m_storageStarted = false;
    bool m_deliveryStarted = false;
};

#endif
