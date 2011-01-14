/****************************************************************************************
 * Copyright (c) 2006 Giovanni Venturi <giovanni@kde-it.org>                            *
 * Copyright (c) 2010 Ralf Engels <ralf-engels@gmx.de>                                  *
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
 
#ifndef AMAROK_EDITFILTERDIALOG_H
#define AMAROK_EDITFILTERDIALOG_H

#include "core/meta/Meta.h"
#include "ui_EditFilterDialog.h"

#include <KDialog>

#include <QList>

class QWidget;

class EditFilterDialog : public KDialog
{
    Q_OBJECT

    public:
        explicit EditFilterDialog( QWidget* parent, const QString &text = QString() );
        ~EditFilterDialog();

        QString filter() const;

    signals:
        void filterChanged( const QString &filter );

    private:
        Ui::EditFilterDialog m_ui;

        bool m_appended;               // true if a filter appended
        QString m_filterText;          // the resulting filter string
        QString m_previousFilterText;  // the previous resulting filter string

    protected slots:
        virtual void slotAttributeChanged();
        virtual void slotAppend();
        virtual void slotClear();
        virtual void slotUndo();
        virtual void slotOk();
};

#endif /* AMAROK_EDITFILTERDIALOG_H */
