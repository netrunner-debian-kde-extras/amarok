/****************************************************************************************
 * Copyright (c) 2007 Dan Meltzer <parallelgrapefruit@gmail.com>                        *
 *                                                                                      *
 * This program is free software; you can redistribute it and/or modify it under        *
 * the terms of the GNU General Public License as published by the Free Software        *
 * Foundation; either version 2 of the License, or (at your option) any later           *
 * version.                                                                             *
 *                                                                                      *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY      *
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A      *
 * PARTICULAR PURPOSE. See the GNU General Public License for more details.             *
 *                                                                                      *
 * You should have received a copy of the GNU General Public License along with         *
 * this program.  If not, see <http://www.gnu.org/licenses/>.                           *
 ****************************************************************************************/

#ifndef SEARCHWIDGET_H
#define SEARCHWIDGET_H

#include "amarok_export.h"
#include "ComboBox.h"

#include <QWidget>

class QToolBar;
class KPushButton;
// A Custom Widget that can be used globally to implement
// searching a treeview.

class AMAROK_EXPORT SearchWidget : public QWidget
{
    Q_OBJECT
    public:
        explicit SearchWidget( QWidget *parent, bool advanced = true );
        SearchWidget( QWidget *parent, QWidget *caller, bool advanced = true  );

        QString currentText() const { return m_sw->currentText(); }
        Amarok::ComboBox *comboBox() { return m_sw; }
        void setup( QObject* caller );
        void setSearchString( const QString &searchString );

        QToolBar * toolBar();

        void showAdvancedButton( bool show );
        
        /**
         * Sets the string that will be visible when the ComboBox's edit text is empty.
         * @param message the string that will be visible when the ComboBox's edit text is empty.
         */
        void setClickMessage( const QString &message );

    signals:
        void filterNow();

    private slots:
        void addCompletion( const QString &text );
        void onComboItemActivated( int index );
        void slotShowFilterEditor();
        void slotFilterEditorFinished( int result );

    private:
        void init( QWidget *parent, bool advanced );

        Amarok::ComboBox *m_sw;
        QAction          *m_filterAction;
        QToolBar         *m_toolBar;
};

#endif
