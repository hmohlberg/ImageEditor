/* 
* Copyright 2026 Forschungszentrum Jülich
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*    https://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*
*/

#pragma once

#include <QString>

/**
 * @brief Abstract interface for the main application window.
 *
 * Provides a global singleton access point that lets core and layer
 * components call back into the GUI without a hard dependency on
 * MainWindow.  MainWindow registers itself via setInstance() at startup.
 */
class IMainSystem {

 public:

    virtual ~IMainSystem() = default;

    /**
     * @brief Pushes an updated operation parameter to the named layer.
     * @param aCaller   Debug string identifying the call site.
     * @param layerName Name of the target layer.
     * @param mode      Parameter type (see LayerItem::OperationMode).
     * @param value1    Primary value.
     * @param value2    Secondary value (optional).
     */
    virtual void updateLayerOperationParameter( const QString &aCaller, const QString &layerName, int mode, double value1, double value2 = 0.0 ) = 0;

    /**
     * @brief Displays a message in the application status bar.
     * @param text    Message text (plain or rich text).
     * @param msgType 0 = normal, 1 = warning (default 0).
     */
    virtual void showMessage( const QString &text, int msgType = 0 ) = 0;

    /**
     * @brief Returns the current operation parameter for the given mode.
     * @param mode  Parameter type (see LayerItem::OperationMode).
     */
    virtual double getLayerOperationParameter( int mode ) = 0;

    /** @brief Returns the name of the layer currently selected in the layer editor. */
    virtual QString getSelectedLayerItemName() = 0;

    /** @brief Returns the singleton instance, or nullptr if not yet registered. */
    static IMainSystem* instance() { return m_instance; }

    /** @brief Registers the singleton instance.  Called by MainWindow at startup. */
    static void setInstance( IMainSystem* inst ) { m_instance = inst; }

 protected:
 
    static IMainSystem* m_instance;
    
};
