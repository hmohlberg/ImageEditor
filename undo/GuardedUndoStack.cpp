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

#include "GuardedUndoStack.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>

GuardedUndoStack::GuardedUndoStack(QObject* parent)
    : QUndoStack(parent)
{}

void GuardedUndoStack::push(QUndoCommand* cmd)
{
    if (m_guardEnabled && m_dialogParent && index() < count()) {
        QDialog dlg(m_dialogParent);
        dlg.setWindowTitle(tr("History conflict"));

        auto* label = new QLabel(tr(
            "The current position is in the middle of the history.\n\n"
            "• Insert here — all subsequent history entries will be discarded.\n"
            "• Append to end — the history remains intact; the new operation is added at the end.\n"
            "• Cancel — the operation is not performed.\n\n"
            "How do you want to proceed?"), &dlg);
        label->setWordWrap(true);

        auto* insertBtn = new QPushButton(tr("Insert here"), &dlg);
        auto* appendBtn = new QPushButton(tr("Append to end"), &dlg);
        auto* cancelBtn = new QPushButton(tr("Cancel"), &dlg);

        insertBtn->setDefault(false);
        appendBtn->setDefault(false);
        cancelBtn->setDefault(true);

        auto* btnLayout = new QHBoxLayout;
        btnLayout->addWidget(insertBtn);
        btnLayout->addWidget(appendBtn);
        btnLayout->addWidget(cancelBtn);

        auto* layout = new QVBoxLayout(&dlg);
        layout->addWidget(label);
        layout->addLayout(btnLayout);

        connect(insertBtn, &QPushButton::clicked, &dlg, [&dlg]{ dlg.done(1); });
        connect(appendBtn, &QPushButton::clicked, &dlg, [&dlg]{ dlg.done(2); });
        connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);

        int result = dlg.exec();

        if (result == 0) {          // cancelled / closed
            delete cmd;
            return;
        }
        if (result == 2) {          // append to end
            setIndex(count());
        }
        // result == 1: insert at current position — fall through to QUndoStack::push
    }
    QUndoStack::push(cmd);
}
