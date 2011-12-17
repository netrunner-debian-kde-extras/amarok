/****************************************************************************************
 * Copyright (c) 2008 Bonne Eggleston <b.eggleston@gmail.com>                           *
 * Copyright (c) 2008 Téo Mrnjavac <teo@kde.org>                                        *
 * Copyright (c) 2010 Casey Link <unnamedrambler@gmail.com>                             *
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

#ifndef AMAROK_ORGANIZECOLLECTIONDIALOG_UI_H
#define AMAROK_ORGANIZECOLLECTIONDIALOG_UI_H

#include "OrganizeCollectionDialog.h"

#include "core/support/Amarok.h"
#include "core/support/Debug.h"
#include "amarokconfig.h"
#include "core-impl/meta/file/File.h"
#include "QStringx.h"
#include "ui_OrganizeCollectionDialogBase.h"
#include "TrackOrganizer.h"
#include <kcolorscheme.h>
#include <KInputDialog>

#include <QApplication>
#include <QDir>
#include <QTimer>

OrganizeCollectionDialog::OrganizeCollectionDialog( const Meta::TrackList &tracks,
                                                    const QStringList &folders,
                                                    const QString &targetExtension,
                                                    QWidget *parent,
                                                    const char *name,
                                                    bool modal,
                                                    const QString &caption,
                                                    QFlags<KDialog::ButtonCode> buttonMask )
    : KDialog( parent )
    , ui( new Ui::OrganizeCollectionDialogBase )
    , m_trackOrganizerDone( false )
    , m_detailed( true )
    , m_schemeModified( false )
    , m_conflict( false )
{
    Q_UNUSED( name )

    setCaption( caption );
    setModal( modal );
    setButtons( buttonMask );
    showButtonSeparator( true );
    m_targetFileExtension = targetExtension;

    if( tracks.size() > 0 )
    {
        m_allTracks = tracks;
    }

    KVBox *mainVBox = new KVBox( this );
    setMainWidget( mainVBox );
    QWidget *mainContainer = new QWidget( mainVBox );

    ui->setupUi( mainContainer );

    m_trackOrganizer = new TrackOrganizer( m_allTracks, this );
    connect( m_trackOrganizer, SIGNAL(finished()), SLOT(slotOrganizerFinished()) );
    //TODO: s/1/enum/g
    //", 1" means isOrganizeCollection ==> doesn't show Options frame
    m_filenameLayoutDialog = new FilenameLayoutDialog( mainContainer, 1 );
    connect( this, SIGNAL( accepted() ),  m_filenameLayoutDialog, SLOT( onAccept() ) );
    ui->verticalLayout->insertWidget( 1, m_filenameLayoutDialog );

    ui->folderCombo->insertItems( 0, folders );
    if( ui->folderCombo->contains( AmarokConfig::organizeDirectory() ) )
        ui->folderCombo->setCurrentItem( AmarokConfig::organizeDirectory() );
    else
        ui->folderCombo->setCurrentIndex( 0 ); //TODO possible bug: assumes folder list is not empty.

    ui->overwriteCheck->setChecked( AmarokConfig::overwriteFiles() );
    m_filenameLayoutDialog->setReplaceSpaces( AmarokConfig::replaceSpace() );
    m_filenameLayoutDialog->setIgnoreThe( AmarokConfig::ignoreThe() );
    m_filenameLayoutDialog->setVfatCompatible( AmarokConfig::vfatCompatible() );
    m_filenameLayoutDialog->setAsciiOnly( AmarokConfig::asciiOnly() );
    m_filenameLayoutDialog->setRegexpText( AmarokConfig::replacementRegexp() );
    m_filenameLayoutDialog->setReplaceText( AmarokConfig::replacementString() );

    ui->previewTableWidget->horizontalHeader()->setResizeMode( QHeaderView::ResizeToContents );
    ui->conflictLabel->setText("");
    QPalette p = ui->conflictLabel->palette();
    KColorScheme::adjustForeground( p, KColorScheme::NegativeText ); // TODO this isn't working, the color is still normal
    ui->conflictLabel->setPalette( p );

    // to show the conflict error
    connect( ui->overwriteCheck, SIGNAL(stateChanged( int )),
             SLOT(slotUpdatePreview()) );
    connect( ui->folderCombo, SIGNAL(currentIndexChanged( const QString & )),
             SLOT(slotUpdatePreview()) );
    connect( m_filenameLayoutDialog, SIGNAL(schemeChanged()), SLOT(slotUpdatePreview()) );

    connect( this, SIGNAL(finished(int)), SLOT(slotSaveFormatList()) );
    connect( this, SIGNAL(accepted()), SLOT(slotDialogAccepted()) );
    connect( ui->folderCombo, SIGNAL(currentIndexChanged( const QString & )),
             SLOT(slotEnableOk( const QString & )) );

    slotEnableOk( ui->folderCombo->currentText() );

    init();
}

OrganizeCollectionDialog::~OrganizeCollectionDialog()
{
    QApplication::restoreOverrideCursor();

    AmarokConfig::setOrganizeDirectory( ui->folderCombo->currentText() );
    delete ui;
}

QMap<Meta::TrackPtr, QString>
OrganizeCollectionDialog::getDestinations()
{
    return m_trackOrganizer->getDestinations();
}

bool
OrganizeCollectionDialog::overwriteDestinations() const
{
    return ui->overwriteCheck->isChecked();
}

QString
OrganizeCollectionDialog::buildFormatTip() const
{
    //FIXME: This is directly copied from mediadevice/generic/genericmediadeviceconfigdialog.ui.h
    QMap<QString, QString> args;
    args["albumartist"] = i18n( "%1 or %2", QLatin1String("Album Artist, The") , QLatin1String("The Album Artist") );
    args["thealbumartist"] = "The Album Artist";
    args["theartist"] = "The Artist";
    args["artist"] = i18n( "%1 or %2", QLatin1String("Artist, The") , QLatin1String("The Artist") );
    args["initial"] = i18n( "Artist's Initial" );
    args["filetype"] = i18n( "File Extension of Source" );
    args["track"] = i18n( "Track Number" );

    QString tooltip = i18n( "<h3>Custom Format String</h3>" );
    tooltip += i18n( "You can use the following tokens:" );
    tooltip += "<ul>";

    for( QMap<QString, QString>::iterator it = args.begin(); it != args.end(); ++it )
        tooltip += QString( "<li>%1 - %%2%" ).arg( it.value(), it.key() );

    tooltip += "</ul>";
    tooltip += i18n( "If you surround sections of text that contain a token with curly-braces, "
            "that section will be hidden if the token is empty." );

    return tooltip;
}


QString
OrganizeCollectionDialog::buildFormatString() const
{
    if( m_filenameLayoutDialog->getParsableScheme().simplified().isEmpty() )
        return "";
    return "%folder%/" + m_filenameLayoutDialog->getParsableScheme() + ".%filetype%";
}

QString
OrganizeCollectionDialog::commonPrefix( const QStringList &list ) const
{
    QString option = list.first().toLower();
    int length = option.length();
    while( length > 0 )
    {
        bool found = true;
        foreach( QString string, list )
        {
            if( string.left(length).toLower() != option )
            {
                found = false;
                break;
            }
        }
        if( found )
            break;
        --length;
        option = option.left( length );
    }
    return option;

}

void
OrganizeCollectionDialog::update( int dummy )   //why the dummy?
{
    Q_UNUSED( dummy );
}


void
OrganizeCollectionDialog::update( const QString & dummy )
{
    Q_UNUSED( dummy );

    update( 0 );
}

void
OrganizeCollectionDialog::init()
{
    slotUpdatePreview();
}

void
OrganizeCollectionDialog::slotUpdatePreview()
{
    QString formatString = buildFormatString();
    m_trackOrganizer->setAsciiOnly( m_filenameLayoutDialog->asciiOnly() );
    m_trackOrganizer->setFolderPrefix( ui->folderCombo->currentText() );
    m_trackOrganizer->setFormatString( formatString );
    m_trackOrganizer->setTargetFileExtension( m_targetFileExtension );
    m_trackOrganizer->setIgnoreThe( m_filenameLayoutDialog->ignoreThe() );
    m_trackOrganizer->setReplaceSpaces( m_filenameLayoutDialog->replaceSpaces() );
    m_trackOrganizer->setReplace( m_filenameLayoutDialog->regexpText(),
                                  m_filenameLayoutDialog->replaceText() );
    m_trackOrganizer->setVfatSafe( m_filenameLayoutDialog->vfatCompatible() );

    //empty the table, not only it's contents
    ui->previewTableWidget->setRowCount( 0 );
    m_conflict = false;
    m_trackOrganizerDone = false;

    QApplication::setOverrideCursor( QCursor( Qt::BusyCursor ) );

    previewNextBatch();
}

void
OrganizeCollectionDialog::previewNextBatch() //private slot
{
    QMap<Meta::TrackPtr, QString> dests = m_trackOrganizer->getDestinations( 10 );
    QMapIterator<Meta::TrackPtr, QString> it( dests );
    while( it.hasNext() )
    {
        it.next();
        Meta::TrackPtr track = it.key();

        QString originalPath = track->prettyUrl();
        QString newPath = it.value();

        int newRow = ui->previewTableWidget->rowCount();
        ui->previewTableWidget->insertRow( newRow );

        //new path preview in the 1st column
        QPalette p = ui->previewTableWidget->palette();
        QTableWidgetItem *item = new QTableWidgetItem( newPath );
        KColorScheme::adjustBackground( p, KColorScheme::NegativeBackground );
        if( QFileInfo( newPath ).exists() )
        {
            item->setBackgroundColor( p.color( QPalette::Base ) );
            m_conflict = true;
        }
        ui->previewTableWidget->setItem( newRow, 0, item );

        //original in the second column
        item = new QTableWidgetItem( originalPath );
        ui->previewTableWidget->setItem( newRow, 1, item );
    }

    if( m_conflict )
    {
        if( ui->overwriteCheck->isChecked() )
            ui->conflictLabel->setText( i18n( "There is a filename conflict, existing files will be overwritten." ) );
        else
            ui->conflictLabel->setText( i18n( "There is a filename conflict, existing files will not be changed." ) );
    }
    else
        ui->conflictLabel->setText(""); // we clear the text instead of hiding it to retain the layout spacing

    //non-blocking way of updating the preview table.
    if( !m_trackOrganizerDone )
        QTimer::singleShot( 0, this, SLOT(previewNextBatch()) );
}

/** WARNING: this slot *has* to be connected with a Qt::DirectConnection to avoid overrun in
  * previewNextBatch()
  */
void
OrganizeCollectionDialog::slotOrganizerFinished()
{
    m_trackOrganizerDone = true;
    QApplication::restoreOverrideCursor();
}

void
OrganizeCollectionDialog::slotDialogAccepted()
{
    AmarokConfig::setOrganizeDirectory( ui->folderCombo->currentText() );
    AmarokConfig::setIgnoreThe( m_filenameLayoutDialog->ignoreThe() );
    AmarokConfig::setReplaceSpace( m_filenameLayoutDialog->replaceSpaces() );
    AmarokConfig::setVfatCompatible( m_filenameLayoutDialog->vfatCompatible() );
    AmarokConfig::setAsciiOnly( m_filenameLayoutDialog->asciiOnly() );
    AmarokConfig::setReplacementRegexp( m_filenameLayoutDialog->regexpText() );
    AmarokConfig::setReplacementString( m_filenameLayoutDialog->replaceText() );

    m_filenameLayoutDialog->onAccept();
}

//The Ok button should be disabled when there's no collection root selected, and when there is no .%filetype in format string
void
OrganizeCollectionDialog::slotEnableOk( const QString & currentCollectionRoot )
{
    if( currentCollectionRoot == 0 )
        enableButtonOk( false );
    else
        enableButtonOk( true );
}

#endif  //AMAROK_ORGANIZECOLLECTIONDIALOG_UI_H
