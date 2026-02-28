#ifndef __IMAGEEDITOR_UNDOSORTDIALOG_H
 #define __IMAGEEDITOR_UNDOSORTDIALOG_H

 #include <QDialog>

 // Forward-Deklarationen
 class QRadioButton;
 class QCheckBox;
 class QComboBox;

 class QUndoSortDialog : public QDialog {
    Q_OBJECT

  public:
  
    enum SortMode { 
        Ascending, 
        Descending, 
        IndexAtEnd 
    };

    struct Options { 
        SortMode sortMode; 
        bool autoMerge; 
        int targetIndex;
    };

    // Konstruktor nimmt nun die Liste der möglichen IDs entgegen
    explicit QUndoSortDialog( const QList<int> &possibleIds, QWidget *parent = nullptr );
    virtual ~QUndoSortDialog();

    Options getOptions() const;

    static Options getUserOptions( const QList<int> &possibleIds, bool *ok = nullptr, QWidget *parent = nullptr );
    
  private slots:
  
    void onSortModeChanged();

  private:
  
    void setupUI( const QList<int> &possibleIds );

    QRadioButton *m_radioAsc;
    QRadioButton *m_radioDesc;
    QRadioButton *m_radioIndexEnd;
    QComboBox *m_indexCombo;
    QCheckBox *m_checkAutoMerge;
 
 };

#endif