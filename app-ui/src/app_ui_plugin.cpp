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
    m_logosAPI = api;
    if (m_logosAPI) {
        m_logos = new LogosModules(m_logosAPI);
    }
    setBackend(this);

    if (!m_logos) return;

    // Subscribe to app_core's statusChanged event; refresh PROPs whenever it fires.
    // Followed by one initial pull so the first paint isn't blank if statusChanged
    // already fired before we subscribed.
    m_logos->app_core.on("statusChanged", [this](const QVariantList&) {
        refresh();
    });
    refresh();
    qDebug() << "app_ui: subscribed to app_core statusChanged + initial refresh";
}

void AppUiPlugin::refresh()
{
    if (!m_logos) return;
    m_logos->app_core.storageStartedAsync(  [this](bool v){ setStorageStarted(v); });
    m_logos->app_core.storageConnectedAsync([this](bool v){ setStorageConnected(v); });
    m_logos->app_core.deliveryStartedAsync( [this](bool v){ setDeliveryStarted(v); });
    m_logos->app_core.deliveryConnectedAsync([this](bool v){ setDeliveryConnected(v); });
}
