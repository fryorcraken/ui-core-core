#include "app_ui_plugin.h"
#include "logos_api.h"
#include <QDebug>

AppUiPlugin::AppUiPlugin(QObject* parent)
    : AppUiSimpleSource(parent) {}

AppUiPlugin::~AppUiPlugin()
{
    delete m_logos;
}

void AppUiPlugin::initLogos(LogosAPI* api)
{
    logosAPI = api;
    m_logosAPI = api;
    if (api) {
        m_logos = new LogosModules(api);
    }
    setBackend(this);

    if (!m_logos) return;

    m_logos->app_core.on("statusChanged", [this](const QVariantList&) {
        qDebug() << "app_ui: statusChanged event received";
        refresh();
    });
    refresh();
    qDebug() << "app_ui: subscribed to app_core statusChanged + initial refresh";
}

void AppUiPlugin::refresh()
{
    if (!m_logos) return;
    m_logos->app_core.storageStartedAsync(  [this](bool v){ qDebug() << "app_ui: storageStarted ="   << v; setStorageStarted(v); });
    m_logos->app_core.storageConnectedAsync([this](bool v){ qDebug() << "app_ui: storageConnected =" << v; setStorageConnected(v); });
    m_logos->app_core.deliveryStartedAsync( [this](bool v){ qDebug() << "app_ui: deliveryStarted ="  << v; setDeliveryStarted(v); });
    m_logos->app_core.deliveryConnectedAsync([this](bool v){ qDebug() << "app_ui: deliveryConnected="<< v; setDeliveryConnected(v); });
}
