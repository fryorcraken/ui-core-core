#ifndef APP_CORE_INTERFACE_H
#define APP_CORE_INTERFACE_H

#include <QObject>
#include "interface.h"

class AppCoreInterface : public PluginInterface
{
public:
    virtual ~AppCoreInterface() = default;

    Q_INVOKABLE virtual bool storageStarted() = 0;
    Q_INVOKABLE virtual bool storageConnected() = 0;
    Q_INVOKABLE virtual bool deliveryStarted() = 0;
    Q_INVOKABLE virtual bool deliveryConnected() = 0;
};

#define AppCoreInterface_iid "org.logos.AppCoreInterface"
Q_DECLARE_INTERFACE(AppCoreInterface, AppCoreInterface_iid)

#endif
