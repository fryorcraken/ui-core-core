#include "app_core_plugin.h"
#include <QDebug>
#include <QFile>
#include <QTextStream>
#include <QDateTime>

static void traceLog(const QString& msg)
{
    QFile f("/tmp/app_core_trace.log");
    if (f.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream s(&f);
        s << QDateTime::currentDateTime().toString(Qt::ISODateWithMs) << " " << msg << "\n";
    }
}

AppCorePlugin::AppCorePlugin(QObject* parent)
    : QObject(parent) { traceLog("ctor"); }

AppCorePlugin::~AppCorePlugin()
{
    delete m_logos;
}

void AppCorePlugin::initLogos(LogosAPI* logosAPIInstance)
{
    traceLog("initLogos called");
    if (!logosAPIInstance) { traceLog("initLogos: api is null, returning"); return; }
    // Assign the inherited PluginInterface::logosAPI member — Part 1 §troubleshooting
    // calls this the "global" pointer; it's actually a public field on PluginInterface.
    // The SDK relies on it for event routing / inter-module calls. Keep m_logosAPI
    // too in case any local code reads it.
    logosAPI = logosAPIInstance;
    m_logosAPI = logosAPIInstance;
    m_logos = new LogosModules(logosAPIInstance);
    qDebug() << "app_core: initLogos start";
    traceLog("initLogos: m_logos created");

    // Storage: init() and start() are synchronous Q_INVOKABLEs returning bool.
    const QString storageCfg = QStringLiteral("{}");
    const bool storageInitOk = m_logos->storage_module.init(storageCfg);
    qDebug() << "app_core: storage_module.init() ->" << storageInitOk;
    if (storageInitOk) {
        m_storageStarted = m_logos->storage_module.start();
        qDebug() << "app_core: storage_module.start() ->" << m_storageStarted;
        traceLog(QString("storage.start -> %1").arg(m_storageStarted));
    }

    // Delivery: async with extended timeout (default 20s is too short for nwaku startup).
    // Also subscribe to connectionStateChanged — when the node actually peers up,
    // delivery emits it with status="Connected" and that's the authoritative signal.
    const QString deliveryCfg = QStringLiteral(
        R"({"logLevel":"DEBUG","mode":"Core","preset":"logos.dev"})");

    m_logos->delivery_module.on("connectionStateChanged",
        [this](const QVariantList& data) {
            const QString status = data.value(0).toString();
            const bool connected = (status == "Connected");
            traceLog(QString("delivery connectionStateChanged: %1 -> connected=%2")
                     .arg(status).arg(connected));
            m_deliveryStarted = connected;
            emit eventResponse("statusChanged", QVariantList{});
        });
    traceLog("delivery: subscribed to connectionStateChanged");

    // Sync createNode/start blocks initLogos for ~21s and the standalone-app's ui-host
    // ready-handshake times out at 10s, so app_ui never loads. Use async with the
    // extended Timeout(60000) so initLogos returns immediately. NB: LogosResult::getError()
    // throws when success=true, so only read it on failure.
    traceLog("delivery.createNodeAsync calling");
    m_logos->delivery_module.createNodeAsync(deliveryCfg,
        [this](LogosResult createRes) {
            traceLog(QString("delivery.createNode cb success=%1%2")
                     .arg(createRes.success)
                     .arg(createRes.success ? "" : QString(" error=%1").arg(createRes.getError())));
            if (!createRes.success) return;
            m_logos->delivery_module.startAsync(
                [this](LogosResult startRes) {
                    traceLog(QString("delivery.start cb success=%1%2")
                             .arg(startRes.success)
                             .arg(startRes.success ? "" : QString(" error=%1").arg(startRes.getError())));
                    // Don't override m_deliveryStarted here — connectionStateChanged is authoritative.
                    emit eventResponse("statusChanged", QVariantList{});
                },
                Timeout(60000));
        },
        Timeout(60000));

    qDebug() << "app_core: initLogos returning (delivery pending async)";
    traceLog(QString("initLogos returning; storageStarted=%1 deliveryStarted=%2 (pending)")
             .arg(m_storageStarted).arg(m_deliveryStarted));

    emit eventResponse("statusChanged", QVariantList{});
}

bool AppCorePlugin::storageStarted()
{
    traceLog(QString("storageStarted getter -> %1").arg(m_storageStarted));
    return m_storageStarted;
}
bool AppCorePlugin::storageConnected()  { return m_storageStarted; }
bool AppCorePlugin::deliveryStarted()
{
    traceLog(QString("deliveryStarted getter -> %1").arg(m_deliveryStarted));
    return m_deliveryStarted;
}
bool AppCorePlugin::deliveryConnected() { return m_deliveryStarted; }
