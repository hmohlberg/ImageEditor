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

#include <QStyledItemDelegate>
#include <QPainter>
#include <QUndoStack>
#include <QUndoView>

// Voraussetzung: Deine AbstractCommand Klasse mit icon() Methode
class DarkHistoryDelegate : public QStyledItemDelegate {

  public:

    explicit DarkHistoryDelegate(QObject *parent = nullptr) : QStyledItemDelegate(parent) {}

    void paint( QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index ) const override {
    
      QStyleOptionViewItem opt = option;
      initStyleOption(&opt, index);
      painter->save();
      painter->setRenderHint(QPainter::Antialiasing);

      // 1. render background
      if ( opt.state & QStyle::State_Selected ) {
        painter->fillRect(opt.rect, QColor("#094771"));
      } else {
        painter->fillRect(opt.rect, QColor("#1e1e1e"));
      }

      // 2. icon-logic with sprecial treatment for index 0
      QIcon displayIcon;
      bool isFuture = !(opt.state & QStyle::State_Enabled);

      if ( index.row() == 0 ) {
        displayIcon = AbstractCommand::getIconFromSvg(
            "<svg viewBox='0 0 64 64'><path d='M12 50 L12 28 L32 12 L52 28 L52 50 Z' fill='white'/></svg>"
        );
      } else {
        if ( auto view = qobject_cast<const QUndoView*>(opt.widget) ) {
            if ( const QUndoStack* stack = view->stack() ) {
                // index.row() - 1, weil der erste Eintrag im View virtuell ist
                const QUndoCommand* cmd = stack->command(index.row() - 1);
                if ( auto absCmd = dynamic_cast<const AbstractCommand*>(cmd) ) {
                    displayIcon = absCmd->icon();
                }
            }
        }
      }

      // 3. draw icon
      if ( !displayIcon.isNull() ) {
        QRect iconRect = opt.rect.adjusted(10, 8, 0, -8);
        iconRect.setWidth(18);
        painter->setOpacity(isFuture ? 0.25 : 0.85);
        displayIcon.paint(painter, iconRect, Qt::AlignCenter, QIcon::Normal);
        painter->setOpacity(1.0);
      }

      // 4. render text
      painter->setPen(isFuture ? QColor("#555555") : (opt.state & QStyle::State_Selected ? Qt::white : QColor("#d0d0d0")));
      if ( isFuture ) {
        QFont f = painter->font();
        f.setItalic(true);
        painter->setFont(f);
      }
      QRect textRect = opt.rect.adjusted(40, 0, -10, 0);
      // check whether text is empty "<empty>"
      QString displayName = (index.row() == 0 && opt.text.contains("<empty>", Qt::CaseInsensitive)) 
                          ? tr("Input image") 
                          : opt.text;
      painter->drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, displayName);
      
      // 5. render timestamp
      if ( index.row() > 0 && !isFuture ) { // show time only for past real commamds
       painter->save();
       QFont timeFont = opt.font;
       timeFont.setPointSizeF(timeFont.pointSize() * 0.8);
       painter->setFont(timeFont);
       painter->setPen(QColor("#666666"));
       if ( auto view = qobject_cast<const QUndoView*>(opt.widget) ) {
        if ( view->stack() ) {
         const QUndoCommand* cmd = view->stack()->command(index.row() - 1);
         if ( auto absCmd = dynamic_cast<const AbstractCommand*>(cmd) ) {
            QString timeStr = absCmd->timeString();
            QRect timeRect = opt.rect.adjusted(0, 0, -10, 0);
            painter->drawText(timeRect, Qt::AlignVCenter | Qt::AlignRight, timeStr);
         }
        }
       }
       painter->restore();
      }

      // 6. separation line
      painter->setPen(QColor("#2d2d2d"));
      painter->drawLine(opt.rect.bottomLeft(), opt.rect.bottomRight());
      painter->restore();
    }

    // defined the height of every entry in the list
    QSize sizeHint( const QStyleOptionViewItem &option, const QModelIndex &index ) const override {
        return QSize(QStyledItemDelegate::sizeHint(option, index).width(), 30);
    }
    
};