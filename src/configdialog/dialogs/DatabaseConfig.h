
/****************************************************************************************
 * Copyright (c) 2009 John Atkinson <john@fauxnetic.co.uk>                              *
 *                                                                                      *
 * This program is free software; you can redistribute it and/or modify it under        *
 * the terms of the GNU General Public License as published by the Free Software        *
 * Foundation; either version 2 of the License, or (at your option) any later           *
 * version.                                                                             *
 *                                                                                      *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY      *
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A      *
 * PARTICULAR PURPOSE. See the GNU General Pulic License for more details.              *
 *                                                                                      *
 * You should have received a copy of the GNU General Public License along with         *
 * this program.  If not, see <http://www.gnu.org/licenses/>.                           *
 ****************************************************************************************/

#ifndef DATABASECONFIG_H
#define DATABASECONFIG_H

#include "ui_DatabaseConfig.h"
#include "ConfigDialogBase.h"


class DatabaseConfig : public ConfigDialogBase, public Ui_DatabaseConfig
{
    Q_OBJECT

    public:
        DatabaseConfig( QWidget* parent );
        virtual ~DatabaseConfig();

        virtual bool hasChanged();
        virtual bool isDefault();
        virtual void updateSettings();

    public slots:
        void toggleExternalConfigAvailable( int checkBoxState );

    private Q_SLOTS:
        void updateSQLQuery();

    private:
        inline bool isSQLInfoPresent() const;
};


#endif


