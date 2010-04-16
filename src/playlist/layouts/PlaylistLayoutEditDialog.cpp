/****************************************************************************************
 * Copyright (c) 2009 Nikolaj Hald Nielsen <nhn@kde.org>                                *
 * Copyright (c) 2009 Téo Mrnjavac <teo@kde.org>                                        *
 * Copyright (c) 2010 Oleksandr Khayrullin <saniokh@gmail.com>                          *
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

#include "PlaylistLayoutEditDialog.h"

#include "core/support/Debug.h"

#include "playlist/layouts/LayoutManager.h"
#include "playlist/PlaylistDefines.h"

#include <KMessageBox>

#include <KInputDialog>
#include <QLineEdit>

Playlist::PlaylistLayoutEditDialog::PlaylistLayoutEditDialog( QWidget *parent )
    : QDialog( parent )
{
    setupUi( this );

    tokenPool->addToken( new Token( i18nc( "'Album' playlist column name and token for playlist layouts", columnNames[Album] ), iconNames[Album], Album ) );

    tokenPool->addToken( new Token( i18nc( "'Album artist' playlist column name and token for playlist layouts", columnNames[AlbumArtist] ), iconNames[AlbumArtist], AlbumArtist ) );

    tokenPool->addToken( new Token( i18nc( "'Artist' playlist column name and token for playlist layouts", columnNames[Artist] ), iconNames[Artist], Artist ) );

    tokenPool->addToken( new Token( i18nc( "'Bitrate' playlist column name and token for playlist layouts", columnNames[Bitrate] ), iconNames[Bitrate], Bitrate ) );

    tokenPool->addToken( new Token( i18nc( "'BPM' playlist column name and token for playlist layouts", columnNames[Bpm] ), iconNames[Bpm], Bpm ) );

    tokenPool->addToken( new Token( i18nc( "'Comment' playlist column name and token for playlist layouts", columnNames[Comment] ), iconNames[Comment], Comment ) );

    tokenPool->addToken( new Token(  i18nc( "'Composer' playlist column name and token for playlist layouts", columnNames[Composer] ), iconNames[Composer], Composer ) );

    tokenPool->addToken( new Token( i18nc( "'Directory' playlist column name and token for playlist layouts", columnNames[Directory] ), iconNames[Directory], Directory ) );

    tokenPool->addToken( new Token( i18nc( "'Disc number' playlist column name and token for playlist layouts", columnNames[DiscNumber] ), iconNames[DiscNumber], DiscNumber ) );

    tokenPool->addToken( new Token( i18nc( "'Divider' token for playlist layouts representing a small visual divider", columnNames[Divider] ), iconNames[Divider], Divider ) );

    tokenPool->addToken( new Token( i18nc( "'File name' playlist column name and token for playlist layouts", columnNames[Filename] ), iconNames[Filename], Filename ) );

    tokenPool->addToken( new Token( i18nc( "'File size' playlist column name and token for playlist layouts", columnNames[Filesize] ), iconNames[Filesize], Filesize ) );

    tokenPool->addToken( new Token( i18nc( "'Genre' playlist column name and token for playlist layouts", columnNames[Genre] ), iconNames[Genre], Genre ) );

    tokenPool->addToken( new Token( i18nc( "'Group length' (total play time of group) playlist column name and token for playlist layouts", columnNames[GroupLength] ), iconNames[GroupLength], GroupLength ) );

    tokenPool->addToken( new Token( i18nc( "'Group tracks' (number of tracks in group) playlist column name and token for playlist layouts", columnNames[GroupTracks] ), iconNames[GroupTracks], GroupTracks ) );

    tokenPool->addToken( new Token( i18nc( "'Last played' (when was track last played) playlist column name and token for playlist layouts", columnNames[LastPlayed] ), iconNames[LastPlayed], LastPlayed ) );

    tokenPool->addToken( new Token( i18nc( "'Labels' (user assigned tags of the track)", columnNames[Labels] ), iconNames[Labels], Labels ) );

    tokenPool->addToken( new Token( i18nc( "'Length' (track length) playlist column name and token for playlist layouts", columnNames[Length] ), iconNames[Length], Length ) );

    tokenPool->addToken( new Token( i18nc( "'Moodbar' playlist column name and token for playlist layouts", "Moodbar", columnNames[Moodbar] ), iconNames[Moodbar], Moodbar ) );

    tokenPool->addToken( new Token( i18nc( "Empty placeholder token used for spacing in playlist layouts", columnNames[PlaceHolder] ), iconNames[PlaceHolder], PlaceHolder ) );

    tokenPool->addToken( new Token( i18nc( "'Play count' playlist column name and token for playlist layouts", columnNames[PlayCount] ), iconNames[PlayCount], PlayCount ) );

    tokenPool->addToken( new Token( i18nc( "'Rating' playlist column name and token for playlist layouts", columnNames[Rating] ), iconNames[Rating], Rating ) );

    tokenPool->addToken( new Token( i18nc( "'Sample rate' playlist column name and token for playlist layouts", columnNames[SampleRate] ), iconNames[SampleRate], SampleRate ) );

    tokenPool->addToken( new Token( i18nc( "'Score' playlist column name and token for playlist layouts", columnNames[Score] ), iconNames[Score], Score ) );

    tokenPool->addToken( new Token( i18nc( "'Source' (local collection, Magnatune.com, last.fm, ... ) playlist column name and token for playlist layouts", columnNames[Source] ), iconNames[Source], Source ) );

    tokenPool->addToken( new Token( i18nc( "'Title' (track name) playlist column name and token for playlist layouts", columnNames[Title] ), iconNames[Title], Title ) );

    tokenPool->addToken( new Token( i18nc( "'Title (with track number)' (track name prefixed with the track number) playlist column name and token for playlist layouts", columnNames[TitleWithTrackNum] ), iconNames[TitleWithTrackNum], TitleWithTrackNum ) );

    tokenPool->addToken( new Token( i18nc( "'Track number' playlist column name and token for playlist layouts", columnNames[TrackNumber] ), iconNames[TrackNumber], TrackNumber ) );

    tokenPool->addToken( new Token(i18nc( "'Type' (file format) playlist column name and token for playlist layouts",  columnNames[Type] ), iconNames[Type], Type ) );

    tokenPool->addToken( new Token( i18nc( "'Year' playlist column name and token for playlist layouts", columnNames[Year] ), iconNames[Year], Year ) );

    m_firstActiveLayout = LayoutManager::instance()->activeLayoutName();

    //add an editor to each tab
    for( int part = 0; part < PlaylistLayout::NumParts; part++ )
        m_partsEdit[part] = new Playlist::LayoutEditWidget( this );
    m_layoutsMap = new QMap<QString, PlaylistLayout>();

    elementTabs->addTab( m_partsEdit[PlaylistLayout::Head], i18n( "Head" ) );
    elementTabs->addTab( m_partsEdit[PlaylistLayout::StandardBody], i18n( "Body" ) );
    elementTabs->addTab( m_partsEdit[PlaylistLayout::VariousArtistsBody], i18n( "Body (Various artists)" ) );
    elementTabs->addTab( m_partsEdit[PlaylistLayout::Single], i18n( "Single" ) );

    QStringList layoutNames = LayoutManager::instance()->layouts();
    foreach( const QString &layoutName, layoutNames )
    {
        PlaylistLayout layout = LayoutManager::instance()->layout( layoutName );
        layout.setDirty( false );
        m_layoutsMap->insert( layoutName, layout );
    }

    layoutListWidget->addItems( layoutNames );

    layoutListWidget->setCurrentRow( LayoutManager::instance()->layouts().indexOf( LayoutManager::instance()->activeLayoutName() ) );

    setupGroupByCombo();

    if ( layoutListWidget->currentItem() )
        setLayout( layoutListWidget->currentItem()->text() );

    connect( previewButton, SIGNAL( clicked() ), this, SLOT( preview() ) );
    connect( layoutListWidget, SIGNAL( currentTextChanged( const QString & ) ), this, SLOT( setLayout( const QString & ) ) );
    connect( layoutListWidget, SIGNAL( currentRowChanged( int ) ), this, SLOT( toggleEditButtons() ) );
    connect( layoutListWidget, SIGNAL( currentRowChanged( int ) ), this, SLOT( toggleUpDownButtons() ) );

    connect( moveUpButton, SIGNAL( clicked() ), this, SLOT( moveUp() ) );
    connect( moveDownButton, SIGNAL( clicked() ), this, SLOT( moveDown() ) );

    buttonBox->button(QDialogButtonBox::Apply)->setIcon( KIcon( "dialog-ok-apply" ) );
    buttonBox->button(QDialogButtonBox::Ok)->setIcon( KIcon( "dialog-ok" ) );
    buttonBox->button(QDialogButtonBox::Cancel)->setIcon( KIcon( "dialog-cancel" ) );
    connect( buttonBox->button(QDialogButtonBox::Apply), SIGNAL( clicked() ), this, SLOT( apply() ) );

    const KIcon newIcon( "document-new" );
    newLayoutButton->setIcon( newIcon );
    newLayoutButton->setToolTip( i18n( "New playlist layout" ) );
    connect( newLayoutButton, SIGNAL( clicked() ), this, SLOT( newLayout() ) );

    const KIcon copyIcon( "edit-copy" );
    copyLayoutButton->setIcon( copyIcon );
    copyLayoutButton->setToolTip( i18n( "Copy playlist layout" ) );
    connect( copyLayoutButton, SIGNAL( clicked() ), this, SLOT( copyLayout() ) );

    const KIcon deleteIcon( "edit-delete" );
    deleteLayoutButton->setIcon( deleteIcon );
    deleteLayoutButton->setToolTip( i18n( "Delete playlist layout" ) );
    connect( deleteLayoutButton, SIGNAL( clicked() ), this, SLOT( deleteLayout() ) );

    const KIcon renameIcon( "edit-rename" );
    renameLayoutButton->setIcon( renameIcon );
    renameLayoutButton->setToolTip( i18n( "Rename playlist layout" ) );
    connect( renameLayoutButton, SIGNAL( clicked() ), this, SLOT( renameLayout() ) );

    toggleEditButtons();
    toggleUpDownButtons();

    for( int part = 0; part < PlaylistLayout::NumParts; part++ )
        connect( m_partsEdit[part], SIGNAL( changed() ), this, SLOT( setLayoutChanged() ) );
    connect( inlineControlsChekbox, SIGNAL( stateChanged( int ) ), this, SLOT( setLayoutChanged() ) );
    connect( tooltipsCheckbox, SIGNAL( stateChanged( int ) ), this, SLOT( setLayoutChanged() ) );
    connect( groupByComboBox, SIGNAL( currentIndexChanged( int ) ), this, SLOT( setLayoutChanged() ) );
}


Playlist::PlaylistLayoutEditDialog::~PlaylistLayoutEditDialog()
{
}

void
Playlist::PlaylistLayoutEditDialog::newLayout()      //SLOT
{
    bool ok;
    QString layoutName = KInputDialog::getText( i18n( "Choose a name for the new playlist layout" ),
                    i18n( "Please enter a name for the playlist layout you are about to define:" ),QString(), &ok, this );
    if( !ok )
	return;
    if( layoutName.isEmpty() )
    {
        KMessageBox::sorry( this, i18n( "Cannot create a layout with no name." ), i18n( "Layout name error" ) );
        return;
    }
    if( m_layoutsMap->keys().contains( layoutName ) )
    {
        KMessageBox::sorry( this, i18n( "Cannot create a layout with the same name as an existing layout." ), i18n( "Layout name error" ) );
        return;
    }
    if( layoutName.contains( '/' ) )
    {
        KMessageBox::sorry( this, i18n( "Cannot create a layout containing '/'." ), i18n( "Layout name error" ) );
        return;
    }

    PlaylistLayout layout;
    layout.setEditable( true );      //Should I use true, TRUE or 1?
    layout.setDirty( true );

    layoutListWidget->addItem( layoutName );
    layoutListWidget->setCurrentItem( (layoutListWidget->findItems( layoutName, Qt::MatchExactly ) ).first() );
    for( int part = 0; part < PlaylistLayout::NumParts; part++ )
    {
        m_partsEdit[part]->clear();
        layout.setLayoutForPart( (PlaylistLayout::Part)part, m_partsEdit[part]->config() );
    }
    m_layoutsMap->insert( layoutName, layout );

    LayoutManager::instance()->addUserLayout( layoutName, layout );

    setLayout( layoutName );
}

void
Playlist::PlaylistLayoutEditDialog::copyLayout()
{
    LayoutItemConfig configs[PlaylistLayout::NumParts];
    for( int part = 0; part < PlaylistLayout::NumParts; part++ )
        configs[part] = m_partsEdit[part]->config();

    QString layoutName = layoutListWidget->currentItem()->text();

    bool ok;
    layoutName = KInputDialog::getText( i18n( "Choose a name for the new playlist layout" ),
            i18n( "Please enter a name for the playlist layout you are about to define as copy of the layout '%1':", layoutName ),
            layoutName, &ok, this );

    if( !ok)
        return;
    if( layoutName.isEmpty() )
    {
        KMessageBox::sorry( this, i18n( "Cannot create a layout with no name." ), i18n( "Layout name error" ) );
        return;
    }
    if( m_layoutsMap->keys().contains( layoutName ) )
    {
        KMessageBox::sorry( this, i18n( "Cannot create a layout with the same name as an existing layout." ), i18n( "Layout name error" ) );
        return;
    }
    //layoutListWidget->addItem( layoutName );
    PlaylistLayout layout;
    layout.setEditable( true );      //Should I use true, TRUE or 1?
    layout.setDirty( true );

    configs[PlaylistLayout::Head].setActiveIndicatorRow( -1 );
    for( int part = 0; part < PlaylistLayout::NumParts; part++ )
        layout.setLayoutForPart( (PlaylistLayout::Part)part, configs[part] );

    layout.setInlineControls( inlineControlsChekbox->isChecked() );
    layout.setTooltips( tooltipsCheckbox->isChecked() );
    layout.setGroupBy( groupByComboBox->itemData( groupByComboBox->currentIndex() ).toString() );

    LayoutManager::instance()->addUserLayout( layoutName, layout );

    //reload from manager:
    layoutListWidget->clear();
    layoutListWidget->addItems( LayoutManager::instance()->layouts() );

    m_layoutsMap->insert( layoutName, layout );
    layoutListWidget->setCurrentItem( ( layoutListWidget->findItems( layoutName, Qt::MatchExactly ) ).first() );
    setLayout( layoutName );
}

void
Playlist::PlaylistLayoutEditDialog::deleteLayout()   //SLOT
{
    m_layoutsMap->remove( layoutListWidget->currentItem()->text() );
    if( LayoutManager::instance()->layouts().contains( layoutListWidget->currentItem()->text() ) )  //if the layout is already saved in the LayoutManager
        LayoutManager::instance()->deleteLayout( layoutListWidget->currentItem()->text() );         //delete it
    delete layoutListWidget->currentItem();
}

void
Playlist::PlaylistLayoutEditDialog::renameLayout()
{
    PlaylistLayout layout = m_layoutsMap->value( layoutListWidget->currentItem()->text() );

    QString layoutName;
    while( layoutName.isEmpty() || m_layoutsMap->keys().contains( layoutName ) )
    {
        bool ok;
        layoutName = KInputDialog::getText( i18n( "Choose a new name for the playlist layout" ),
                    i18n( "Please enter a new name for the playlist layout you are about to rename:" ),
                    layoutListWidget->currentItem()->text(), &ok, this);
        if ( !ok )
        {
            //Cancelled so just return
            return;
        }
        if( layoutName.isEmpty() )
            KMessageBox::sorry( this, i18n( "Cannot rename a layout to have no name." ), i18n( "Layout name error" ) );
        if( m_layoutsMap->keys().contains( layoutName ) )
            KMessageBox::sorry( this, i18n( "Cannot rename a layout to have the same name as an existing layout." ), i18n( "Layout name error" ) );
    }
    m_layoutsMap->remove( layoutListWidget->currentItem()->text() );
    if( LayoutManager::instance()->layouts().contains( layoutListWidget->currentItem()->text() ) )  //if the layout is already saved in the LayoutManager
        LayoutManager::instance()->deleteLayout( layoutListWidget->currentItem()->text() );         //delete it
    LayoutManager::instance()->addUserLayout( layoutName, layout );
    m_layoutsMap->insert( layoutName, layout );
    layoutListWidget->currentItem()->setText( layoutName );

    setLayout( layoutName );
}

/**
 * Loads the configuration of the layout layoutName from the m_layoutsMap to the LayoutItemConfig area.
 * @param layoutName the name of the PlaylistLayout to be loaded for configuration
 */
void
Playlist::PlaylistLayoutEditDialog::setLayout( const QString &layoutName )   //SLOT
{
    DEBUG_BLOCK
    m_layoutName = layoutName;

    if( m_layoutsMap->keys().contains( layoutName ) )   //is the layout exists in the list of loaded layouts
    {
        PlaylistLayout layout = m_layoutsMap->value( layoutName );
        for( int part = 0; part < PlaylistLayout::NumParts; part++ )
            m_partsEdit[part]->readLayout( layout.layoutForPart( (PlaylistLayout::Part)part ) );
        inlineControlsChekbox->setChecked( layout.inlineControls() );
        tooltipsCheckbox->setChecked( layout.tooltips() );
        groupByComboBox->setCurrentIndex( groupByComboBox->findData( layout.groupBy() ) );
        setEnabledTabs();
        //make sure that it is not marked dirty (it will be because of the changed signal triggereing when loagin it)
        //unless it is actually changed
        debug() << "not dirty anyway!!";
        (*m_layoutsMap)[m_layoutName].setDirty( false );
    }
    else
    {
        for( int part = 0; part < PlaylistLayout::NumParts; part++ )
            m_partsEdit[part]->clear();
    }
}

void
Playlist::PlaylistLayoutEditDialog::preview()
{
    PlaylistLayout layout;

    for( int part = 0; part < PlaylistLayout::NumParts; part++ )
    {
        LayoutItemConfig config = m_partsEdit[part]->config();
        if( part == PlaylistLayout::Head )
            config.setActiveIndicatorRow( -1 );
        layout.setLayoutForPart( (PlaylistLayout::Part)part, config );
    }

    layout.setInlineControls( inlineControlsChekbox->isChecked() );
    layout.setTooltips( tooltipsCheckbox->isChecked() );
    layout.setGroupBy( groupByComboBox->itemData( groupByComboBox->currentIndex() ).toString() );

    LayoutManager::instance()->setPreviewLayout( layout );
}

void
Playlist::PlaylistLayoutEditDialog::toggleEditButtons() //SLOT
{
    if ( !layoutListWidget->currentItem() ) {
        deleteLayoutButton->setEnabled( 0 );
        renameLayoutButton->setEnabled( 0 );
    } else if( LayoutManager::instance()->isDefaultLayout( layoutListWidget->currentItem()->text() ) ) {
        deleteLayoutButton->setEnabled( 0 );
        renameLayoutButton->setEnabled( 0 );
    } else {
        deleteLayoutButton->setEnabled( 1 );
        renameLayoutButton->setEnabled( 1 );
    }
}

void
Playlist::PlaylistLayoutEditDialog::toggleUpDownButtons()
{
    if ( !layoutListWidget->currentItem() )
    {
        moveUpButton->setEnabled( 0 );
        moveDownButton->setEnabled( 0 );
    }
    else if ( layoutListWidget->currentRow() == 0 )
    {
        moveUpButton->setEnabled( 0 );
        if ( layoutListWidget->currentRow() >= m_layoutsMap->size() -1 )
            moveDownButton->setEnabled( 0 );
        else
            moveDownButton->setEnabled( 1 );
    }
    else if ( layoutListWidget->currentRow() >= m_layoutsMap->size() -1 )
    {
        moveDownButton->setEnabled( 0 );
        moveUpButton->setEnabled( 1 ); //we already cheked that this is not row 0
    }
    else
    {
        moveDownButton->setEnabled( 1 );
        moveUpButton->setEnabled( 1 );
    }


}

void
Playlist::PlaylistLayoutEditDialog::apply()  //SLOT
{
    QMap<QString, PlaylistLayout>::Iterator i = m_layoutsMap->begin();
    while( i != m_layoutsMap->end() )
    {
        if( i.value().isDirty() )
        {
            QString layoutName = i.key();
            if ( LayoutManager::instance()->isDefaultLayout( i.key() ) )
            {
                QString newLayoutName = i18n( "copy of %1", layoutName );
                QString orgCopyName = newLayoutName;

                int copyNumber = 1;
                QStringList existingLayouts = LayoutManager::instance()->layouts();
                while( existingLayouts.contains( newLayoutName ) )
                {
                    copyNumber++;
                    newLayoutName = i18nc( "adds a copy number to a generated name if the name already exists, for instance 'copy of Foo 2' if 'copy of Foo' is taken", "%1 %2", orgCopyName, copyNumber );
                }

                const QString msg = i18n( "The layout '%1' you modified is one of the default layouts and cannot be overwritten. "
                                          "Saved as new layout '%2'", layoutName, newLayoutName );
                KMessageBox::sorry( this, msg, i18n( "Default Layout" ) );

                layoutName = newLayoutName;

                layoutListWidget->addItem( layoutName );
                layoutListWidget->setCurrentItem( ( layoutListWidget->findItems( layoutName, Qt::MatchExactly ) ).first() );
                m_layoutsMap->insert( layoutName, i.value() );
                setLayout( layoutName );
            }
            i.value().setInlineControls( inlineControlsChekbox->isChecked() );
            i.value().setTooltips( tooltipsCheckbox->isChecked() );
            i.value().setGroupBy( groupByComboBox->itemData( groupByComboBox->currentIndex() ).toString() );
            i.value().setDirty( false );
            LayoutManager::instance()->addUserLayout( layoutName, i.value() );
        }
        i++;
    }
    LayoutManager::instance()->setActiveLayout( layoutListWidget->currentItem()->text() );  //important to override the previewed layout if preview is used
}

void
Playlist::PlaylistLayoutEditDialog::accept()     //SLOT
{
    apply();
    QDialog::accept();
}

void
Playlist::PlaylistLayoutEditDialog::reject()     //SLOT
{
    DEBUG_BLOCK

    debug() << "Applying initial layout: " << m_firstActiveLayout;
    if( layoutListWidget->findItems( m_firstActiveLayout, Qt::MatchExactly ).isEmpty() )
        LayoutManager::instance()->setActiveLayout( "Default" );
    else
        LayoutManager::instance()->setActiveLayout( m_firstActiveLayout );

    QDialog::reject();
}

void
Playlist::PlaylistLayoutEditDialog::moveUp()
{
    int newRow = LayoutManager::instance()->moveUp( m_layoutName );

    layoutListWidget->clear();
    layoutListWidget->addItems( LayoutManager::instance()->layouts() );

    layoutListWidget->setCurrentRow( newRow );
}

void
Playlist::PlaylistLayoutEditDialog::moveDown()
{
    int newRow = LayoutManager::instance()->moveDown( m_layoutName );

    layoutListWidget->clear();
    layoutListWidget->addItems( LayoutManager::instance()->layouts() );

    layoutListWidget->setCurrentRow( newRow );
}

void
Playlist::PlaylistLayoutEditDialog::setEnabledTabs()
{
    DEBUG_BLOCK

    //Enable or disable tabs depending on whether grouping is allowed.
    QString grouping = groupByComboBox->itemData( groupByComboBox->currentIndex() ).toString();
    bool groupingEnabled = ( !grouping.isEmpty() && grouping != "None" );

    if ( !groupingEnabled )
        elementTabs->setCurrentWidget( m_partsEdit[PlaylistLayout::Single] );

    debug() << groupByComboBox->itemData( groupByComboBox->currentIndex() ).toString();
    debug() << groupingEnabled;

    elementTabs->setTabEnabled( elementTabs->indexOf( m_partsEdit[PlaylistLayout::Head] ), groupingEnabled );
    elementTabs->setTabEnabled( elementTabs->indexOf( m_partsEdit[PlaylistLayout::StandardBody] ), groupingEnabled );
    elementTabs->setTabEnabled( elementTabs->indexOf( m_partsEdit[PlaylistLayout::VariousArtistsBody] ), groupingEnabled );
}

//Sets up a combo box that presents the possible grouping categories, as well as the option
//to perform no grouping.
//We'll use the "user data" to store the un-i18n-ized category name for internal use.
void
Playlist::PlaylistLayoutEditDialog::setupGroupByCombo()
{
    foreach ( const QString &it, Playlist::groupableCategories )
    {
        QString prettyCategoryName = columnNames.at( internalColumnNames.indexOf( it ) );
        QString iconName = iconNames.at( internalColumnNames.indexOf( it ) );
        groupByComboBox->addItem( KIcon( iconName ), prettyCategoryName, QVariant( it ) );
    }

    //Add the option to not perform grouping
    //Use a null string to specify "no grouping"
    groupByComboBox->addItem( i18n( "No Grouping" ), QVariant( "None" ) );
}

void
Playlist::PlaylistLayoutEditDialog::setLayoutChanged()
{
    DEBUG_BLOCK

    setEnabledTabs();

    for( int part = 0; part < PlaylistLayout::NumParts; part++ )
        (*m_layoutsMap)[m_layoutName].setLayoutForPart( (PlaylistLayout::Part)part, m_partsEdit[part]->config() );

    (*m_layoutsMap)[m_layoutName].setInlineControls( inlineControlsChekbox->isChecked() );
    (*m_layoutsMap)[m_layoutName].setTooltips( tooltipsCheckbox->isChecked() );
    (*m_layoutsMap)[m_layoutName].setGroupBy( groupByComboBox->itemData( groupByComboBox->currentIndex() ).toString() );
    (*m_layoutsMap)[m_layoutName].setDirty( true );
}

#include "PlaylistLayoutEditDialog.moc"

