#ifndef APP_UI_INTERFACE_H
#define APP_UI_INTERFACE_H

#include <QObject>
#include <QString>
#include "interface.h"

/**
 * @brief Interface for the UI Example module
 *
 * UI modules extend PluginInterface and provide createWidget()/destroyWidget()
 * to supply the host application with a QWidget* for display.
 */
class AppUiInterface : public PluginInterface
{
public:
    virtual ~AppUiInterface() = default;
};

#define AppUiInterface_iid "org.logos.AppUiInterface"
Q_DECLARE_INTERFACE(AppUiInterface, AppUiInterface_iid)

#endif // APP_UI_INTERFACE_H
