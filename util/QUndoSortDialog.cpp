#include "QUndoSortDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QRadioButton>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QDialogButtonBox>
#include <QStyledItemDelegate>

QUndoSortDialog::QUndoSortDialog( const QList<int> &possibleIds, QWidget *parent ) 
    : QDialog(parent) 
{
    setupUI(possibleIds);
}

QUndoSortDialog::~QUndoSortDialog() {}

void QUndoSortDialog::setupUI( const QList<int> &possibleIds ) 
{
    setWindowTitle(tr("ImageEditor - Sorting and merging history entries"));
    setModal(true);
    setMinimumWidth(490);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // Sortiergruppe
    QGroupBox *sortGroup = new QGroupBox(tr("In what order should the entries be arranged in the history?"), this);
    QVBoxLayout *sortLayout = new QVBoxLayout(sortGroup);

    m_radioAsc = new QRadioButton(tr("Ascending order"), this);
    m_radioDesc = new QRadioButton(tr("Descending order"), this);
    m_radioIndexEnd = new QRadioButton(tr("Move specific layer index to the end:"), this);
    
    QString radioStyle = 
      "QRadioButton { color: #eee; }"
      "QRadioButton::indicator { width: 10px; height: 10px; }"
      "QRadioButton::indicator:checked { background-color: #0078d7; border: 2px solid #eee; border-radius: 7px; }"
      "QRadioButton::indicator:unchecked { background-color: #333; border: 2px solid #777; border-radius: 7px; }";
    m_radioAsc->setStyleSheet(radioStyle);
    m_radioDesc->setStyleSheet(radioStyle);
    m_radioIndexEnd->setStyleSheet(radioStyle);
    
    m_radioAsc->setChecked(true);

    // Layout für die Index-Auswahl (Radio-Button + ComboBox nebeneinander)
    QHBoxLayout *indexSelectionLayout = new QHBoxLayout();
    m_indexCombo = new QComboBox(this);
    m_indexCombo->setItemDelegate(new QStyledItemDelegate(m_indexCombo));
    m_indexCombo->setMaxVisibleItems(10);
    m_indexCombo->setFixedWidth(80);
    m_indexCombo->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_indexCombo->clear();
    for ( int id : possibleIds ) {
        QString text = QString::number(id);
        if ( !text.isEmpty() ) {
          m_indexCombo->addItem(text, id);
        }
    }
    m_indexCombo->setEnabled(false); // Erst aktivieren, wenn RadioButton gewählt

    indexSelectionLayout->addWidget(m_radioIndexEnd);
    indexSelectionLayout->addWidget(m_indexCombo);
    indexSelectionLayout->addStretch();

    sortLayout->addWidget(m_radioAsc);
    sortLayout->addWidget(m_radioDesc);
    sortLayout->addLayout(indexSelectionLayout);
    mainLayout->addWidget(sortGroup);

    // Signal-Slot Verbindung für die Aktivierung der ComboBox
    connect(m_radioAsc, &QRadioButton::toggled, this, &QUndoSortDialog::onSortModeChanged);
    connect(m_radioDesc, &QRadioButton::toggled, this, &QUndoSortDialog::onSortModeChanged);
    connect(m_radioIndexEnd, &QRadioButton::toggled, this, &QUndoSortDialog::onSortModeChanged);

    // Merging Option
    m_checkAutoMerge = new QCheckBox(tr("Enable automatic merging where possible (no undo available)"), this);
    m_checkAutoMerge->setChecked(true);
    m_checkAutoMerge->setEnabled(false);
    m_checkAutoMerge->setStyleSheet(
      "QCheckBox { color: #eee; }"
      "QCheckBox::indicator {"
      "    width: 10px; height: 10px;"
      "    border: 1px solid #777; background: #222;"
      "}"
      "QCheckBox::indicator:checked {"
      "    background: #0078d7;" // Ein kräftiges Blau für das Häkchen-Feld
      "    border: 1px solid #fff;"
      "}"
    );
    mainLayout->addWidget(m_checkAutoMerge);

    // Buttons
    QDialogButtonBox *buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttonBox);
}

void QUndoSortDialog::onSortModeChanged() 
{
    // ComboBox nur aktiv, wenn m_radioIndexEnd ausgewählt ist
    m_indexCombo->setEnabled(m_radioIndexEnd->isChecked());
}

QUndoSortDialog::Options QUndoSortDialog::getOptions() const 
{
    Options opt;
    if (m_radioDesc->isChecked()) opt.sortMode = Descending;
    else if (m_radioIndexEnd->isChecked()) opt.sortMode = IndexAtEnd;
    else opt.sortMode = Ascending;

    opt.autoMerge = m_checkAutoMerge->isChecked();
    opt.targetIndex = m_indexCombo->currentData().toInt(); // Wert aus ComboBox lesen
    
    return opt;
}

QUndoSortDialog::Options QUndoSortDialog::getUserOptions( const QList<int> &possibleIds, bool *ok, QWidget *parent ) 
{
    QUndoSortDialog dialog(possibleIds, parent);
    bool accepted = (dialog.exec() == QDialog::Accepted);
    if (ok) *ok = accepted;
    return dialog.getOptions();
}