/****************************************************************************************
 * Copyright (c) 2010 Nikolaj Hald Nielsen <nhn@kde.org>                                *
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

#ifndef FILEBROWSERMKII_H
#define FILEBROWSERMKII_H

#include "BrowserCategory.h"

#include <KUrl>

class QModelIndex;

class FileBrowser : public BrowserCategory
{
    Q_OBJECT

public:
    FileBrowser( const char *name, QWidget *parent );
    ~FileBrowser();

    virtual void setupAddItems();
    virtual void polish();

    virtual QString prettyName() const;

    /**
    * Navigate to a specific directory
    */
    void setDir( const KUrl &dir );

    /**
     * Return the path of the currently shown dir.
     */
    QString currentDir() const;

protected slots:
    void itemActivated( const QModelIndex &index );
    
    void slotSetFilterTimeout();
    void slotFilterNow();

    void addItemActivated( const QString &callback );

    virtual void reActivate();

    /**
     * Shows/hides the columns as selected in the context menu of the header of the
     * file view.
     * @param toggled the visibility state of a column in the context menu.
     */
    void toggleColumn( bool toggled );

    /**
     * Go backward in history
     */
    void back();

    /**
     * Go forward in history
     */
    void forward();

    /**
     * Navigates up one level in the path shown
     */
    void up();

    /**
     * Navigates to home directory
     */
    void home();

    /**
     * Navigates to "places"
     */
    void showPlaces();

    /**
     * Handle results of tryiong to setup an item in "places" that needed mouting or other
     * special setup.
     * @param index the index that we tried to setup
     * @param success did the setup succeed?
     */
    void setupDone( const QModelIndex &index, bool success );

private slots:
    void initView();

private:
    class Private;
    Private *const d;

    Q_PRIVATE_SLOT( d, void slotSaveHeaderState() )
};

#endif // FILEBROWSERMKII_H
