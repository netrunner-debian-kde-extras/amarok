/****************************************************************************************
 * Copyright (c) 2007 Ian Monroe <ian@monroe.nu>                                        *
 * Copyright (c) 2008-2009 Dan Meltzer <parallelgrapefruit@gmail.com>                   *
 * Copyright (c) 2011 Ralf Engels <ralf-engels@gmx.de>                                  *
 *                                                                                      *
 * This program is free software; you can redistribute it and/or modify it under        *
 * the terms of the GNU General Public License as published by the Free Software        *
 * Foundation; either version 2 of the License, or (at your option) version 3 or        *
 * any later version accepted by the membership of KDE e.V. (or its successor approved  *
 * by the membership of KDE e.V.), which shall act as a proxy defined in Section 14 of  *
 * version 3 of the license.                                                            *
 *                                                                                      *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY      *
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A      *
 * PARTICULAR PURPOSE. See the GNU General Public License for more details.             *
 *                                                                                      *
 * You should have received a copy of the GNU General Public License along with         *
 * this program.  If not, see <http://www.gnu.org/licenses/>.                           *
 ****************************************************************************************/

#define DEBUG_PREFIX "CollectionWidget"

#include "CollectionWidget.h"

#include "CollectionTreeItemModel.h"
#include "CollectionTreeItemModelBase.h"
#include "CollectionTreeItemDelegate.h"
#include "CollectionBrowserTreeView.h"
#include "core-impl/collections/support/CollectionManager.h"
#include "core-impl/collections/proxycollection/ProxyCollection.h"
#include "core/support/Debug.h"
#include "SearchWidget.h"
#include "SingleCollectionTreeItemModel.h"
#include <amarokconfig.h>
#include "core/meta/support/MetaConstants.h"

#include <KAction>
#include <KIcon>
#include <KLocale>
#include <KMenu>
#include <KMenuBar>
#include <KStandardDirs>
#include <KStandardGuiItem>

#include <QActionGroup>
#include <QMetaEnum>
#include <QMetaObject>
#include <QRect>
#include <QSortFilterProxyModel>
#include <QStackedWidget>
#include <QToolBar>
#include <QToolButton>

CollectionWidget *CollectionWidget::s_instance = 0;

#define CATEGORY_LEVEL_COUNT 3

class CollectionWidget::Private
{
public:
    Private()
        : treeView( 0 )
        , singleTreeView( 0 )
        , viewMode( CollectionWidget::NormalCollections ) {}
    ~Private() {}

    CollectionBrowserTreeView *view( CollectionWidget::ViewMode mode );

    CollectionBrowserTreeView *treeView;
    CollectionBrowserTreeView *singleTreeView;
    QStackedWidget *stack;
    SearchWidget *searchWidget;
    CollectionWidget::ViewMode viewMode;

    QMenu *menuLevel[CATEGORY_LEVEL_COUNT];
    QActionGroup *levelGroups[CATEGORY_LEVEL_COUNT];
};

CollectionBrowserTreeView *
CollectionWidget::Private::view( CollectionWidget::ViewMode mode )
{
    CollectionBrowserTreeView *v(0);

    switch( mode )
    {
    case CollectionWidget::NormalCollections:
        if( !treeView )
        {
            v = new CollectionBrowserTreeView( stack );
            v->setAlternatingRowColors( true );
            v->setFrameShape( QFrame::NoFrame );
            v->setRootIsDecorated( false );
            connect( v, SIGNAL(leavingTree()), searchWidget->comboBox(), SLOT(setFocus()) );
            CollectionTreeItemDelegate *delegate = new CollectionTreeItemDelegate( v );
            v->setItemDelegate( delegate );
            CollectionTreeItemModelBase *multiModel = new CollectionTreeItemModel( QList<int>() );
            v->setModel( multiModel );
            treeView = v;
        }
        else
        {
            v = treeView;
        }
        break;

    case CollectionWidget::UnifiedCollection:
        if( !singleTreeView )
        {
            v = new CollectionBrowserTreeView( stack );
            v->setAlternatingRowColors( true );
            v->setFrameShape( QFrame::NoFrame );
            Collections::ProxyCollection *proxyColl = new Collections::ProxyCollection();
            connect( CollectionManager::instance(),
                     SIGNAL(collectionAdded(Collections::Collection*,CollectionManager::CollectionStatus)),
                     proxyColl,
                     SLOT(addCollection(Collections::Collection*,CollectionManager::CollectionStatus)));
            connect( CollectionManager::instance(), SIGNAL(collectionRemoved(QString)),
                     proxyColl, SLOT(removeCollection(QString)));
            foreach( Collections::Collection* coll, CollectionManager::instance()->viewableCollections() )
            {
                proxyColl->addCollection( coll, CollectionManager::CollectionViewable );
            }
            CollectionTreeItemModelBase *singleModel = new SingleCollectionTreeItemModel( proxyColl, QList<int>() );
            v->setModel( singleModel );
            singleTreeView = v;
        }
        else
        {
            v = singleTreeView;
        }
        break;
    }
    return v;
}

CollectionWidget::CollectionWidget( const QString &name , QWidget *parent )
    : BrowserCategory( name, parent )
    , d( new Private )
{
    s_instance = this;
    setObjectName( name );
    setMargin( 0 );
    setSpacing( 0 );
    //TODO: we have a really nice opportunity to make these info blurbs both helpful and pretty
    setLongDescription( i18n( "This is where you will find your local music, as well as music from mobile audio players and CDs." ) );
    setImagePath( KStandardDirs::locate( "data", "amarok/images/hover_info_collections.png" ) );

    // set background
    const QString bgImage = KStandardDirs::locate("data", "amarok/images/hover_info_collections.png");
    setBackgroundImage( bgImage );

    KHBox *hbox = new KHBox( this );
    d->searchWidget = new SearchWidget( hbox );
    d->searchWidget->setClickMessage( i18n( "Search collection" ) );
    d->stack = new QStackedWidget( this );

    QTimer::singleShot( 0, this, SLOT(init()) );
}

CollectionWidget::~CollectionWidget()
{
    delete d;
}

void
CollectionWidget::init()
{
    DEBUG_BLOCK
    PERF_LOG( "Begin init" );

    QList<int>levels = Amarok::config( "Collection Browser" ).readEntry( "TreeCategory", QList<int>() );
    if ( levels.isEmpty() )
        levels << CategoryId::Artist << CategoryId::Album;

    const QMetaObject *mo = metaObject();
    const QMetaEnum me = mo->enumerator( mo->indexOfEnumerator( "ViewMode" ) );
    const QString &value = Amarok::config( "Collection Browser" ).readEntry( "View Mode" );
    int enumValue = me.keyToValue( value.toLocal8Bit().constData() );
    enumValue == -1 ? d->viewMode = NormalCollections : d->viewMode = (ViewMode) enumValue;

    d->stack->setFrameShape( QFrame::NoFrame );
    d->stack->addWidget( d->view( d->viewMode ) );

    // Filter presets. UserRole is used to store the actual syntax.
    KComboBox *combo = d->searchWidget->comboBox();
    const KIcon icon = KStandardGuiItem::find().icon();
    combo->addItem( icon, i18nc("@item:inlistbox Collection widget filter preset", "Added This Hour"),
                    QString(Meta::shortI18nForField( Meta::valCreateDate ) + ":<1h") );
    combo->addItem( icon, i18nc("@item:inlistbox Collection widget filter preset", "Added Today"),
                    QString(Meta::shortI18nForField( Meta::valCreateDate ) + ":<1d") );
    combo->addItem( icon, i18nc("@item:inlistbox Collection widget filter preset", "Added This Week"),
                    QString(Meta::shortI18nForField( Meta::valCreateDate ) + ":<1w") );
    combo->addItem( icon, i18nc("@item:inlistbox Collection widget filter preset", "Added This Month"),
                    QString(Meta::shortI18nForField( Meta::valCreateDate ) + ":<1m") );
    combo->insertSeparator( combo->count() );

    QMenu *filterMenu = new QMenu( 0 );

    QAction *action = new QAction( i18n( "Artist / Album" ), this );
    connect( action, SIGNAL( triggered( bool ) ), SLOT( sortByArtistAlbum() ) );
    filterMenu->addAction( action );

    action = new QAction( i18n( "Album / Artist" ), this );
    connect( action, SIGNAL( triggered( bool ) ), SLOT( sortByAlbumArtist() ) );
    filterMenu->addAction( action );

    action = new QAction( i18n( "Genre / Artist" ), this );
    connect( action, SIGNAL( triggered( bool ) ), SLOT( sortByGenreArtist() ) );
    filterMenu->addAction( action );

    action = new QAction( i18n( "Genre / Artist / Album" ), this );
    connect( action, SIGNAL(triggered( bool ) ), SLOT( sortByGenreArtistAlbum() ) );
    filterMenu->addAction( action );

    filterMenu->addSeparator();

    // -- generate the level menus
    d->menuLevel[0] = filterMenu->addMenu( i18n( "First Level" ) );
    d->menuLevel[1] = filterMenu->addMenu( i18n( "Second Level" ) );
    d->menuLevel[2] = filterMenu->addMenu( i18n( "Third Level" ) );

    // - fill the level menus
    for( int i = 0; i < CATEGORY_LEVEL_COUNT; i++ )
    {
        QActionGroup *actionGroup = new QActionGroup( this );

        if( i > 0 )
        {
            QAction *action = d->menuLevel[i]->addAction( i18n( "None" ) );
            action->setData( CategoryId::None );
            action->setCheckable( true );
            actionGroup->addAction( action );

            if( levels.count() <= i )
                action->setChecked( true );
        }

        // - and now the different selections
        struct LevelDefinition {
            qint64 field;
            CategoryId::CatMenuId menuId;
        };
        LevelDefinition definitions[] = { {Meta::valArtist, CategoryId::Artist},
                                          {Meta::valAlbum, CategoryId::Album},
                                          {Meta::valAlbumArtist, CategoryId::AlbumArtist},
                                          {Meta::valGenre, CategoryId::Genre},
                                          {Meta::valComposer, CategoryId::Composer},
                                          {Meta::valLabel, CategoryId::Label} };

        for( unsigned int j = 0; j < sizeof( definitions ) / sizeof( definitions[0] ); j++ )
        {
            QAction *action = d->menuLevel[i]->addAction( Meta::i18nForField( definitions[j].field ) );
            action->setData( definitions[j].menuId );
            action->setCheckable( true );
            actionGroup->addAction( action );

            if( levels.count() > i && levels[i] == definitions[j].menuId )
                action->setChecked( true );
        }

        d->levelGroups[i] = actionGroup;
    }

    connect( d->menuLevel[0], SIGNAL( triggered( QAction *) ), SLOT( sortLevelSelected( QAction * ) ) );
    connect( d->menuLevel[1], SIGNAL( triggered( QAction *) ), SLOT( sortLevelSelected( QAction * ) ) );
    connect( d->menuLevel[2], SIGNAL( triggered( QAction *) ), SLOT( sortLevelSelected( QAction * ) ) );


    // -- create the checkboxes

    filterMenu->addSeparator();
    QAction *showYears = filterMenu->addAction( i18n( "Show Years" ) );
    showYears->setCheckable( true );
    showYears->setChecked( AmarokConfig::showYears() );
    connect( showYears, SIGNAL( toggled( bool ) ), SLOT( slotShowYears( bool ) ) );

    QAction *showTrackNumbers = filterMenu->addAction( i18nc("@action:inmenu", "Show Track Numbers") );
    showTrackNumbers->setCheckable( true );
    showTrackNumbers->setChecked( AmarokConfig::showTrackNumbers() );
    connect( showTrackNumbers, SIGNAL( toggled( bool ) ), SLOT( slotShowTrackNumbers( bool ) ) );

    QAction *showCovers = filterMenu->addAction( i18n( "Show Cover Art" ) );
    showCovers->setCheckable( true );
    showCovers->setChecked( AmarokConfig::showAlbumArt() );
    connect( showCovers, SIGNAL(toggled(bool)), SLOT( slotShowCovers( bool ) ) );


    d->searchWidget->toolBar()->addSeparator();

    KAction *toggleAction = new KAction( KIcon( "view-list-tree" ), i18n( "Merged View" ), this );
    toggleAction->setCheckable( true );
    toggleAction->setChecked( d->viewMode == CollectionWidget::UnifiedCollection );
    toggleView( d->viewMode == CollectionWidget::UnifiedCollection );
    connect( toggleAction, SIGNAL( triggered( bool ) ), SLOT( toggleView( bool ) ) );
    d->searchWidget->toolBar()->addAction( toggleAction );

    KAction *searchMenuAction = new KAction( KIcon( "preferences-other" ), i18n( "Sort Options" ), this );
    searchMenuAction->setMenu( filterMenu );
    d->searchWidget->toolBar()->addAction( searchMenuAction );

    QToolButton *tbutton = qobject_cast<QToolButton*>( d->searchWidget->toolBar()->widgetForAction( searchMenuAction ) );
    if( tbutton )
        tbutton->setPopupMode( QToolButton::InstantPopup );

    setLevels( levels );
}

void
CollectionWidget::sortLevelSelected( QAction *action )
{
    Q_UNUSED( action );

    QList<int> levels;
    for( int i = 0; i < CATEGORY_LEVEL_COUNT; i++ )
    {
        if( d->levelGroups[i]->checkedAction() )
        {
            int category = d->levelGroups[i]->checkedAction()->data().toInt();
            if( category != CategoryId::None )
                levels << category;
        }
    }
    setLevels( levels );
}

void
CollectionWidget::sortByArtistAlbum()
{
    QList<int> levels;
    levels << CategoryId::Artist << CategoryId::Album;
    setLevels( levels );
}

void
CollectionWidget::sortByAlbumArtist()
{
    QList<int> levels;
    levels << CategoryId::Album << CategoryId::Artist;
    setLevels( levels );
}

void
CollectionWidget::sortByGenreArtist()
{
    QList<int> levels;
    levels << CategoryId::Genre << CategoryId::Artist;
    setLevels( levels );
}

void
CollectionWidget::sortByGenreArtistAlbum()
{
    QList<int> levels;
    levels << CategoryId::Genre << CategoryId::Artist << CategoryId::Album;
    setLevels( levels );
}

void
CollectionWidget::slotShowYears( bool checked )
{
    AmarokConfig::setShowYears( checked );
    setLevels( levels() );
}

void
CollectionWidget::slotShowTrackNumbers( bool checked )
{
    AmarokConfig::setShowTrackNumbers( checked );
    setLevels( levels() );
}

void
CollectionWidget::slotShowCovers(bool checked)
{
    AmarokConfig::setShowAlbumArt( checked );
    setLevels( levels() );
}

QString
CollectionWidget::filter() const
{
    return d->searchWidget->currentText();
}

void CollectionWidget::setFilter( const QString &filter )
{
    d->searchWidget->setSearchString( filter );
}

QList<int>
CollectionWidget::levels() const
{
    // return const_cast<CollectionWidget*>( this )->view( d->viewMode )->levels();
    return d->view( d->viewMode )->levels();
}

void CollectionWidget::setLevels( const QList<int> &levels )
{
    // -- select the corrrect menu entries
    for( int i = 0; i < CATEGORY_LEVEL_COUNT; i++ )
    {
        int category;
        if( levels.count() > i )
            category = levels[i];
        else
            category = CategoryId::None;

        foreach( QAction* action, d->levelGroups[i]->actions() )
        {
            if( action->data().toInt() == category && !action->isChecked() )
                action->setChecked( true );
        }
    }

    // -- set the levels in the view
    d->view( d->viewMode )->setLevels( levels );
    debug() << "Sort levels:" << levels;
}

void CollectionWidget::toggleView( bool merged )
{
    CollectionWidget::ViewMode newMode = merged ? UnifiedCollection : NormalCollections;
    CollectionBrowserTreeView *oldView = d->view( d->viewMode );

    if( oldView )
        d->searchWidget->disconnect( oldView );

    CollectionBrowserTreeView *newView = d->view( newMode );
    d->searchWidget->setup( newView );
    if( d->stack->indexOf( newView ) == -1 )
        d->stack->addWidget( newView );
    d->stack->setCurrentWidget( newView );
    const QString &filter = d->searchWidget->currentText();
    if( !filter.isEmpty() )
    {
        typedef CollectionTreeItemModelBase CTIMB;
        CTIMB *model = qobject_cast<CTIMB*>( newView->filterModel()->sourceModel() );
        model->setCurrentFilter( filter );
        newView->slotFilterNow();
    }

    d->viewMode = newMode;
    if( oldView )
        setLevels( oldView->levels() );

    const QMetaObject *mo = metaObject();
    const QMetaEnum me = mo->enumerator( mo->indexOfEnumerator( "ViewMode" ) );
    Amarok::config( "Collection Browser" ).writeEntry( "View Mode", me.valueToKey( d->viewMode ) );
}

#include "CollectionWidget.moc"
