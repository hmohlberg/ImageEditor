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

#include <QUndoStack>

class QWidget;

// Wraps QUndoStack::push() with a dialog when the current index is not at the
// end of the stack (i.e. the user is in the middle of the history).  The user
// can choose to insert at the current position (default Qt behaviour, discards
// forward entries), append to the end of the stack, or cancel the operation.
class GuardedUndoStack : public QUndoStack
{
    Q_OBJECT

public:
    explicit GuardedUndoStack(QObject* parent = nullptr);

    void setDialogParent(QWidget* widget) { m_dialogParent = widget; }

    // Disable the guard temporarily (e.g. during batch/loading operations).
    void setGuardEnabled(bool enabled) { m_guardEnabled = enabled; }
    bool guardEnabled() const          { return m_guardEnabled; }

    // Shadows (does NOT override) QUndoStack::push().  All callers that hold a
    // GuardedUndoStack* will use this method; callers with a raw QUndoStack*
    // bypass it, so all shared pointers must be typed GuardedUndoStack*.
    void push(QUndoCommand* cmd);

private:
    QWidget* m_dialogParent = nullptr;
    bool     m_guardEnabled = true;
};
