/****************************************************************************************
 * Copyright (c) 2007 Alexandre Pereira de Oliveira <aleprj@gmail.com>                  *
 * Copyright (c) 2007-2009 Maximilian Kossick <maximilian.kossick@googlemail.com>       *
 * Copyright (c) 2007 Nikolaj Hald Nielsen <nhn@kde.org>                                *
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

#define DEBUG_PREFIX "CollectionTreeItemModelBase"

#include "CollectionTreeItemModelBase.h"

#include "core/support/Amarok.h"
#include "AmarokMimeData.h"
#include "core/collections/Collection.h"
#include "CollectionTreeItem.h"
#include "core/support/Debug.h"
#include "Expression.h"
#include "core/meta/support/MetaConstants.h"
#include "core/collections/QueryMaker.h"
#include "amarokconfig.h"
#include "core/capabilities/EditCapability.h"
#include "shared/FileType.h"
#include "SvgHandler.h"

#include <KGlobalSettings>
#include <KIcon>
#include <KIconLoader>
#include <KLocale>
#include <KStandardDirs>

#include <QFontMetrics>
#include <QPixmap>
#include <QTimeLine>
#include <QTimer>

using namespace Meta;

inline uint qHash( const Meta::DataPtr &data )
{
    return qHash( data.data() );
}


CollectionTreeItemModelBase::CollectionTreeItemModelBase( )
    : QAbstractItemModel()
    , m_rootItem( 0 )
    , d( new Private )
    , m_animFrame( 0 )
    , m_loading1( QPixmap( KStandardDirs::locate("data", "amarok/images/loading1.png" ) ) )
    , m_loading2( QPixmap( KStandardDirs::locate("data", "amarok/images/loading2.png" ) ) )
    , m_currentAnimPixmap( m_loading1 )
{
    m_timeLine = new QTimeLine( 10000, this );
    m_timeLine->setFrameRange( 0, 20 );
    m_timeLine->setLoopCount ( 0 );
    updateRowHeight();
    connect( m_timeLine, SIGNAL( frameChanged( int ) ), this, SLOT( loadingAnimationTick() ) );
    connect( KGlobalSettings::self(), SIGNAL(kdisplayFontChanged()), SLOT(updateRowHeight()) );
}

CollectionTreeItemModelBase::~CollectionTreeItemModelBase()
{
    delete m_rootItem;
    delete d;
}

Qt::ItemFlags CollectionTreeItemModelBase::flags(const QModelIndex & index) const
{
    Qt::ItemFlags flags = 0;
    if( index.isValid() )
    {
        flags = Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsEditable;
    }
    return flags;

}

bool
CollectionTreeItemModelBase::setData( const QModelIndex &index, const QVariant &value, int role )
{
    Q_UNUSED( role )

    if( !index.isValid() )
        return false;
    CollectionTreeItem *item = static_cast<CollectionTreeItem*>( index.internalPointer() );

    Meta::DataPtr data = item->data();

    if( Meta::TrackPtr track = Meta::TrackPtr::dynamicCast( data ) )
    {
        if( !track->hasCapabilityInterface( Capabilities::Capability::Editable ) )
            return false;
        Capabilities::EditCapability *ec = track->create<Capabilities::EditCapability>();
        if( ec )
        {
            ec->setTitle( value.toString() );
            emit dataChanged( index, index );
            delete ec;
            return true;
        }
    }
    else if( Meta::AlbumPtr album = Meta::AlbumPtr::dynamicCast( data ) )
    {
        Meta::TrackList tracks = album->tracks();
        if( !tracks.isEmpty() )
        {
            foreach( Meta::TrackPtr track, tracks )
            {
                QScopedPointer<Capabilities::EditCapability> ec( track->create<Capabilities::EditCapability>() );
                if( ec )
                    ec->setAlbum( value.toString() );
            }
            emit dataChanged( index, index );
            return true;
        }
    }
    else if( Meta::ArtistPtr artist = Meta::ArtistPtr::dynamicCast( data ) )
    {
        Meta::TrackList tracks = artist->tracks();
        if( !tracks.isEmpty() )
        {
            foreach( Meta::TrackPtr track, tracks )
            {
                Capabilities::EditCapability *ec = track->create<Capabilities::EditCapability>();
                if( ec )
                    ec->setArtist( value.toString() );
                delete ec;
            }
            emit dataChanged( index, index );
            return true;
        }
    }
    else if( Meta::GenrePtr genre = Meta::GenrePtr::dynamicCast( data ) )
    {
        Meta::TrackList tracks = genre->tracks();
        if( !tracks.isEmpty() )
        {
            foreach( Meta::TrackPtr track, tracks )
            {
                Capabilities::EditCapability *ec = track->create<Capabilities::EditCapability>();
                if( ec )
                    ec->setGenre( value.toString() );
                delete ec;
            }
            emit dataChanged( index, index );
            return true;
        }
    }
    else if( Meta::YearPtr year = Meta::YearPtr::dynamicCast( data ) )
    {
        Meta::TrackList tracks = year->tracks();
        if( !tracks.isEmpty() )
        {
            foreach( Meta::TrackPtr track, tracks )
            {
                Capabilities::EditCapability *ec = track->create<Capabilities::EditCapability>();
                if( ec )
                    ec->setYear( value.toInt() );
                delete ec;
            }
            emit dataChanged( index, index );
            return true;
        }
    }
    else if( Meta::ComposerPtr composer = Meta::ComposerPtr::dynamicCast( data ) )
    {
        Meta::TrackList tracks = composer->tracks();
        if( !tracks.isEmpty() )
        {
            foreach( Meta::TrackPtr track, tracks )
            {
                Capabilities::EditCapability *ec = track->create<Capabilities::EditCapability>();
                if( ec )
                    ec->setComposer( value.toString() );
                delete ec;
            }
            emit dataChanged( index, index );
            return true;
        }
    }
    return false;
}

QVariant
CollectionTreeItemModelBase::dataForItem( CollectionTreeItem *item, int role, int level ) const
{
    // -- do the decoration and the size hint role
    if( role == Qt::SizeHintRole )
    {
        QSize size( 1, d->rowHeight );
        if( item->isAlbumItem() && AmarokConfig::showAlbumArt() )
        {
            if( d->rowHeight < 34 )
                size.setHeight( 34 );
        }
        return size;
    }

    if( level == -1 )
        level = item->level();

    if( item->isTrackItem() )
    {
        Meta::TrackPtr track = Meta::TrackPtr::dynamicCast( item->data() );
        switch( role )
        {
        case Qt::DisplayRole:
        case Qt::ToolTipRole:
        case CustomRoles::FilterRole:
            {
                QString name = track->prettyName();
                Meta::AlbumPtr album = track->album();
                Meta::ArtistPtr artist = track->artist();

                if( album && artist && album->isCompilation() )
                    name.prepend( QString("%1 - ").arg(artist->prettyName()) );

                if( AmarokConfig::showTrackNumbers() )
                {
                    int trackNum = track->trackNumber();
                    if( trackNum > 0 )
                        name.prepend( QString("%1. ").arg(trackNum) );
                }

                // Check empty after track logic and before album logic
                if( name.isEmpty() )
                    name = i18nc( "The Name is not known", "Unknown" );
                return name;
            }

        case Qt::DecorationRole:
            return KIcon( "media-album-track" );
        case CustomRoles::SortRole:
            return track->sortableName();
        }
    }
    else if( item->isAlbumItem() )
    {
        Meta::AlbumPtr album = Meta::AlbumPtr::dynamicCast( item->data() );
        switch( role )
        {
        case Qt::DisplayRole:
        case Qt::ToolTipRole:
            {
                QString name = album->prettyName();
                if( AmarokConfig::showYears() )
                {
                    Meta::TrackList tracks = album->tracks();
                    if( !tracks.isEmpty() )
                    {
                        Meta::YearPtr year = tracks.first()->year();
                        if( year && (year->year() != 0) )
                            name.prepend( QString("%1 - ").arg( year->name() ) );
                    }
                }
                return name;
            }

        case Qt::DecorationRole:
            if( AmarokConfig::showAlbumArt() )
                return The::svgHandler()->imageWithBorder( album, 32, 2 );
            else
                return iconForLevel( level );

        case CustomRoles::SortRole:
            return album->sortableName();
        }
    }
    else if( item->isDataItem() )
    {
        switch( role )
        {
        case Qt::DisplayRole:
        case Qt::ToolTipRole:
        case CustomRoles::FilterRole:
            {
                QString name = item->data()->prettyName();
                if( name.isEmpty() )
                    name = i18nc( "The Name is not known", "Unknown" );
                return name;
            }

        case Qt::DecorationRole:
            {
                if( d->childQueries.values().contains( item ) )
                {
                    if( level < m_levelType.count() )
                        return m_currentAnimPixmap;
                }
                return iconForLevel( level );
            }

        case CustomRoles::SortRole:
            return item->data()->sortableName();
        }
    }
    else if( item->isVariousArtistItem() )
    {
        switch( role )
        {
        case Qt::DecorationRole:
            return KIcon( "similarartists-amarok" );
        case Qt::DisplayRole:
            return i18n( "Various Artists" );
        case CustomRoles::SortRole:
            return QString(); // so that we can compare it against other strings
        }
    }

    // -- all other roles are handled by item
    return item->data( role );
}

QVariant
CollectionTreeItemModelBase::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
    {
        if (section == 0)
            return m_headerText;
    }
    return QVariant();
}

QModelIndex
CollectionTreeItemModelBase::index(int row, int column, const QModelIndex & parent) const
{
    //ensure sanity of parameters
    //we are a tree model, there are no columns
    if( row < 0 || column != 0 )
        return QModelIndex();

    CollectionTreeItem *parentItem;

    if (!parent.isValid())
        parentItem = m_rootItem;
    else
        parentItem = static_cast<CollectionTreeItem*>(parent.internalPointer());

    CollectionTreeItem *childItem = parentItem->child(row);
    if( childItem )
        return createIndex(row, column, childItem);
    else
        return QModelIndex();
}

QModelIndex
CollectionTreeItemModelBase::parent(const QModelIndex & index) const
{
     if( !index.isValid() )
         return QModelIndex();

     CollectionTreeItem *childItem = static_cast<CollectionTreeItem*>(index.internalPointer());
     CollectionTreeItem *parentItem = childItem->parent();

     if ( (parentItem == m_rootItem) || !parentItem )
         return QModelIndex();

     return createIndex(parentItem->row(), 0, parentItem);
}

int
CollectionTreeItemModelBase::rowCount(const QModelIndex & parent) const
{
    CollectionTreeItem *parentItem;

    if( !parent.isValid() )
        parentItem = m_rootItem;
    else
        parentItem = static_cast<CollectionTreeItem*>(parent.internalPointer());

    return parentItem->childCount();
}

int CollectionTreeItemModelBase::columnCount(const QModelIndex & parent) const
{
    Q_UNUSED( parent )
    return 1;
}

QStringList
CollectionTreeItemModelBase::mimeTypes() const
{
    QStringList types;
    types << AmarokMimeData::TRACK_MIME;
    return types;
}

QMimeData*
CollectionTreeItemModelBase::mimeData(const QModelIndexList & indices) const
{
    if ( indices.isEmpty() )
        return 0;

    QList<CollectionTreeItem*> items;

    foreach( const QModelIndex &index, indices )
    {
        if( index.isValid() )
            items << static_cast<CollectionTreeItem*>( index.internalPointer() );
    }

    return mimeData( items );
}

QMimeData*
CollectionTreeItemModelBase::mimeData(const QList<CollectionTreeItem*> & items) const
{
    if ( items.isEmpty() )
        return 0;

    Meta::TrackList tracks;
    QList<Collections::QueryMaker*> queries;

    foreach( CollectionTreeItem *item, items )
    {
        if( item->allDescendentTracksLoaded() ) {
            tracks << item->descendentTracks();
        }
        else
        {
            Collections::QueryMaker *qm = item->queryMaker();
            CollectionTreeItem *tmpItem = item;
            while( tmpItem->isDataItem() )
            {
                if( tmpItem->data() )
                {
                    if( levelCategory( tmpItem->level() - 1 ) == CategoryId::AlbumArtist )
                        qm->setArtistQueryMode( Collections::QueryMaker::AlbumArtists );
                    tmpItem->addMatch( qm );
                }
                else
                    qm->setAlbumQueryMode( Collections::QueryMaker::OnlyCompilations );
                tmpItem = tmpItem->parent();
            }
            addFilters( qm );
            queries.append( qm );
        }
    }

    qStableSort( tracks.begin(), tracks.end(), Meta::Track::lessThan );

    AmarokMimeData *mimeData = new AmarokMimeData();
    mimeData->setTracks( tracks );
    mimeData->setQueryMakers( queries );
    mimeData->startQueries();
    return mimeData;
}

bool
CollectionTreeItemModelBase::hasChildren ( const QModelIndex & parent ) const
{
     if( !parent.isValid() )
         return true; // must be root item!

    CollectionTreeItem *item = static_cast<CollectionTreeItem*>(parent.internalPointer());
    //we added the collection level so we have to be careful with the item level
    return !item->isDataItem() || item->level() + levelModifier() <= m_levelType.count();

}

void
CollectionTreeItemModelBase::ensureChildrenLoaded( CollectionTreeItem *item )
{
    //only start a query if necessary and we are not querying for the item's children already
    if ( item->requiresUpdate() && !d->runningQueries.contains( item ) )
    {
        listForLevel( item->level() + levelModifier(), item->queryMaker(), item );
    }
}

QIcon
CollectionTreeItemModelBase::iconForLevel(int level) const
{
    switch( m_levelType[level] )
    {
        case CategoryId::Album :
            return KIcon( "media-optical-amarok" );
        case CategoryId::Artist :
            return KIcon( "view-media-artist-amarok" );
        case CategoryId::AlbumArtist :
            return KIcon( "amarok_artist" );
        case CategoryId::Composer :
            return KIcon( "filename-composer-amarok" );
        case CategoryId::Genre :
            return KIcon( "favorite-genres-amarok" );
        case CategoryId::Year :
            return KIcon( "clock" );
        case CategoryId::Label :
            return KIcon( "label-amarok" );
        case CategoryId::None :
        default:
            return KIcon( "image-missing" );
    }
}

void CollectionTreeItemModelBase::listForLevel(int level, Collections::QueryMaker * qm, CollectionTreeItem * parent)
{
    if( qm && parent )
    {
        // this check should not hurt anyone... needs to check if single... needs it
        if( d->runningQueries.contains( parent ) )
            return;

        // - check if we are finished
        if( level > m_levelType.count() ||
            parent->isVariousArtistItem() ||
            parent->isNoLabelItem() )
        {
            qm->deleteLater();
            return;
        }

        // - the last level are always the tracks
        if ( level == m_levelType.count() )
            qm->setQueryType( Collections::QueryMaker::Track );

        // - all other levels are more complicate
        else
        {
            Collections::QueryMaker::QueryType nextLevel;
            if( level + 1 >= m_levelType.count() )
                nextLevel = Collections::QueryMaker::Track;
            else
                nextLevel = mapCategoryToQueryType( m_levelType [ level + 1 ] );

            qm->setQueryType( mapCategoryToQueryType( m_levelType[ level ] ) );

            switch( m_levelType[level] )
            {
                case CategoryId::Album :
                    //restrict query to normal albums if the previous level
                    //was the artist category. in that case we handle compilations below
                    if( level > 0 &&
                        ( m_levelType[level-1] == CategoryId::Artist ||
                          m_levelType[level-1] == CategoryId::AlbumArtist
                        ) &&
                        !parent->isVariousArtistItem() )
                    {
                        qm->setAlbumQueryMode( Collections::QueryMaker::OnlyNormalAlbums );
                    }
                    break;

                case CategoryId::Artist :
                case CategoryId::AlbumArtist:
                    //handle compilations only if the next level ist CategoryId::Album
                    if( nextLevel == Collections::QueryMaker::Album )
                    {
                        handleCompilations( parent );
                        qm->setAlbumQueryMode( Collections::QueryMaker::OnlyNormalAlbums );
                    }
                    break;

                case CategoryId::Label:
                    handleTracksWithoutLabels( nextLevel, parent );
                    break;

                default : //TODO handle error condition. return tracks?
                    break;
            }
        }

        for( CollectionTreeItem *tmpItem = parent; tmpItem->parent(); tmpItem = tmpItem->parent() )
        {
            // a various artist item means we are looking for compilations here
            if( tmpItem->isVariousArtistItem() )
                qm->setAlbumQueryMode( Collections::QueryMaker::OnlyCompilations );
            else if( tmpItem->data() )
            {
                if( levelCategory( tmpItem->level() - 1 ) == CategoryId::AlbumArtist )
                    qm->setArtistQueryMode( Collections::QueryMaker::AlbumArtists );
                 tmpItem->addMatch( qm );
            }
        }
        addFilters( qm );
        qm->setReturnResultAsDataPtrs( true );
        connect( qm, SIGNAL( newResultReady( QString, Meta::DataList ) ), SLOT( newResultReady( QString, Meta::DataList ) ), Qt::QueuedConnection );
        connect( qm, SIGNAL( queryDone() ), SLOT( queryDone() ), Qt::QueuedConnection );
        d->childQueries.insert( qm, parent );
        d->runningQueries.insert( parent, qm );
        qm->run();

        //some very quick queries may be done so fast that the loading
        //animation creates an unnecessary flicker, therefore delay it for a bit
        QTimer::singleShot( 150, this, SLOT( startAnimationTick() ) );
    }
}

Collections::QueryMaker::QueryType
CollectionTreeItemModelBase::mapCategoryToQueryType( int levelType ) const
{
    Collections::QueryMaker::QueryType type;
    switch( levelType )
    {
    case CategoryId::Album:
        type = Collections::QueryMaker::Album;
        break;
    case CategoryId::Artist:
        type = Collections::QueryMaker::Artist;
        break;
    case CategoryId::AlbumArtist:
        type = Collections::QueryMaker::AlbumArtist;
        break;
    case CategoryId::Composer:
        type = Collections::QueryMaker::Composer;
        break;
    case CategoryId::Genre:
        type = Collections::QueryMaker::Genre;
        break;
    case CategoryId::Label:
        type = Collections::QueryMaker::Label;
        break;
    case CategoryId::Year:
        type = Collections::QueryMaker::Year;
        break;
    default:
        type = Collections::QueryMaker::None;
        break;
    }

    return type;
}


#define ADD_OR_EXCLUDE_FILTER( VALUE, FILTER, MATCHBEGIN, MATCHEND ) \
            { if( elem.negate ) \
                qm->excludeFilter( VALUE, FILTER, MATCHBEGIN, MATCHEND ); \
            else \
                qm->addFilter( VALUE, FILTER, MATCHBEGIN, MATCHEND ); }
#define ADD_OR_EXCLUDE_NUMBER_FILTER( VALUE, FILTER, COMPARE ) \
            { if( elem.negate ) \
                qm->excludeNumberFilter( VALUE, FILTER, COMPARE ); \
            else \
                qm->addNumberFilter( VALUE, FILTER, COMPARE ); }

void
CollectionTreeItemModelBase::addDateFilter( qint64 field, Collections::QueryMaker::NumberComparison compare, const expression_element &elem, Collections::QueryMaker *qm ) const
{
    if( compare == Collections::QueryMaker::Equals )
    {
        const uint dateCutOff = semanticDateTimeParser( elem.text ).toTime_t();
        if( dateCutOff > 0 )
        {
            ADD_OR_EXCLUDE_NUMBER_FILTER( field, dateCutOff, Collections::QueryMaker::GreaterThan );
        }
    }
    else
    {
        Collections::QueryMaker::NumberComparison compareAlt = Collections::QueryMaker::GreaterThan;
        if( compare == Collections::QueryMaker::GreaterThan )
        {
            qm->endAndOr();
            qm->beginAnd();
            compareAlt = Collections::QueryMaker::LessThan;
            ADD_OR_EXCLUDE_NUMBER_FILTER( field, 0, compare );
        }
        const uint time_t = semanticDateTimeParser( elem.text ).toTime_t();
        ADD_OR_EXCLUDE_NUMBER_FILTER( field, time_t, compareAlt );
    }
}

void
CollectionTreeItemModelBase::addFilters( Collections::QueryMaker *qm ) const
{
    int validFilters = qm->validFilterMask();

    ParsedExpression parsed = ExpressionParser::parse ( m_currentFilter );
    foreach( const or_list &orList, parsed )
    {
        qm->beginOr();

        foreach ( const expression_element &elem, orList )
        {
            if( elem.negate )
                qm->beginAnd();
            else
                qm->beginOr();

            if ( elem.field.isEmpty() )
            {
                qm->beginOr();

                if( ( validFilters & Collections::QueryMaker::TitleFilter ) )
                    ADD_OR_EXCLUDE_FILTER( Meta::valTitle, elem.text, false, false );
                if( ( validFilters & Collections::QueryMaker::UrlFilter ) )
                    ADD_OR_EXCLUDE_FILTER( Meta::valUrl, elem.text, false, false );
                if( ( validFilters & Collections::QueryMaker::AlbumFilter ) )
                    ADD_OR_EXCLUDE_FILTER( Meta::valAlbum, elem.text, false, false );
                if( ( validFilters & Collections::QueryMaker::ArtistFilter ) )
                    ADD_OR_EXCLUDE_FILTER( Meta::valArtist, elem.text, false, false );
                if( ( validFilters & Collections::QueryMaker::AlbumArtistFilter ) )
                    ADD_OR_EXCLUDE_FILTER( Meta::valAlbumArtist, elem.text, false, false );
                if( ( validFilters & Collections::QueryMaker::ComposerFilter ) )
                    ADD_OR_EXCLUDE_FILTER( Meta::valComposer, elem.text, false, false );
                if( ( validFilters & Collections::QueryMaker::GenreFilter ) )
                    ADD_OR_EXCLUDE_FILTER( Meta::valGenre, elem.text, false, false );
                if( ( validFilters & Collections::QueryMaker::YearFilter ) )
                    ADD_OR_EXCLUDE_FILTER( Meta::valYear, elem.text, false, false );

                ADD_OR_EXCLUDE_FILTER( Meta::valLabel, elem.text, false, false );

                qm->endAndOr();
            }
            else
            {
                //get field values based on name
                QString lcField = elem.field.toLower();
                Collections::QueryMaker::NumberComparison compare = Collections::QueryMaker::Equals;
                switch( elem.match )
                {
                    case expression_element::More:
                        compare = Collections::QueryMaker::GreaterThan;
                        break;
                    case expression_element::Less:
                        compare = Collections::QueryMaker::LessThan;
                        break;
                    case expression_element::Contains:
                        compare = Collections::QueryMaker::Equals;
                        break;
                }

                // TODO: Once we have MetaConstants.cpp use those functions here
                if ( lcField.compare( "album", Qt::CaseInsensitive ) == 0 || lcField.compare( shortI18nForField( Meta::valAlbum ), Qt::CaseInsensitive ) == 0 )
                {
                    if ( ( validFilters & Collections::QueryMaker::AlbumFilter ) == 0 ) continue;
                    ADD_OR_EXCLUDE_FILTER( Meta::valAlbum, elem.text, false, false );
                }
                else if ( lcField.compare( "artist", Qt::CaseInsensitive ) == 0 || lcField.compare( shortI18nForField( Meta::valArtist ), Qt::CaseInsensitive ) == 0 )
                {
                    if ( ( validFilters & Collections::QueryMaker::ArtistFilter ) == 0 ) continue;
                    ADD_OR_EXCLUDE_FILTER( Meta::valArtist, elem.text, false, false );
                }
                else if ( lcField.compare( "albumartist", Qt::CaseInsensitive ) == 0 || lcField.compare( shortI18nForField( Meta::valAlbumArtist ), Qt::CaseInsensitive ) == 0 )
                {
                    if ( ( validFilters & Collections::QueryMaker::AlbumArtistFilter ) == 0 ) continue;
                    ADD_OR_EXCLUDE_FILTER( Meta::valAlbumArtist, elem.text, false, false );
                }
                else if ( lcField.compare( "genre", Qt::CaseInsensitive ) == 0 || lcField.compare( shortI18nForField( Meta::valGenre ), Qt::CaseInsensitive ) == 0)
                {
                    if ( ( validFilters & Collections::QueryMaker::GenreFilter ) == 0 ) continue;
                    ADD_OR_EXCLUDE_FILTER( Meta::valGenre, elem.text, false, false );
                }
                else if ( lcField.compare( "title", Qt::CaseInsensitive ) == 0 || lcField.compare( shortI18nForField( Meta::valTitle ), Qt::CaseInsensitive ) == 0 )
                {
                    if ( ( validFilters & Collections::QueryMaker::TitleFilter ) == 0 ) continue;
                    ADD_OR_EXCLUDE_FILTER( Meta::valTitle, elem.text, false, false );
                }
                else if ( lcField.compare( "composer", Qt::CaseInsensitive ) == 0 || lcField.compare( shortI18nForField( Meta::valComposer ), Qt::CaseInsensitive ) == 0 )
                {
                    if ( ( validFilters & Collections::QueryMaker::ComposerFilter ) == 0 ) continue;
                    ADD_OR_EXCLUDE_FILTER( Meta::valComposer, elem.text, false, false );
                }
                else if( lcField.compare( "label", Qt::CaseInsensitive ) == 0 || lcField.compare( shortI18nForField( Meta::valLabel ), Qt::CaseInsensitive ) == 0 )
                {
                    ADD_OR_EXCLUDE_FILTER( Meta::valLabel, elem.text, false, false );
                }
                else if ( lcField.compare( "year", Qt::CaseInsensitive ) == 0 || lcField.compare( shortI18nForField( Meta::valYear ), Qt::CaseInsensitive ) == 0)
                {
                    if ( ( validFilters & Collections::QueryMaker::YearFilter ) == 0 ) continue;
                    ADD_OR_EXCLUDE_NUMBER_FILTER( Meta::valYear, elem.text.toInt(), compare );
                }
                else if ( lcField.compare( "bpm", Qt::CaseInsensitive ) == 0 || lcField.compare( shortI18nForField( Meta::valBpm ), Qt::CaseInsensitive ) == 0)
                {
                    ADD_OR_EXCLUDE_NUMBER_FILTER( Meta::valBpm, elem.text.toInt(), compare );
                }
                else if( lcField.compare( "comment", Qt::CaseInsensitive ) == 0 || lcField.compare( shortI18nForField( Meta::valComment ), Qt::CaseInsensitive ) == 0 )
                {
                    ADD_OR_EXCLUDE_FILTER( Meta::valComment, elem.text, false, false );
                }
                else if( lcField.compare( "filename", Qt::CaseInsensitive ) == 0 || lcField.compare( shortI18nForField( Meta::valUrl ), Qt::CaseInsensitive ) == 0 )
                {
                    ADD_OR_EXCLUDE_FILTER( Meta::valUrl, elem.text, false, false );
                }
                else if( lcField.compare( "bitrate", Qt::CaseInsensitive ) == 0 || lcField.compare( shortI18nForField( Meta::valBitrate ), Qt::CaseInsensitive ) == 0 )
                {
                    ADD_OR_EXCLUDE_NUMBER_FILTER( Meta::valBitrate, elem.text.toInt(), compare );
                }
                else if( lcField.compare( "rating", Qt::CaseInsensitive ) == 0 || lcField.compare( shortI18nForField( Meta::valRating ), Qt::CaseInsensitive ) == 0 )
                {
                    ADD_OR_EXCLUDE_NUMBER_FILTER( Meta::valRating, elem.text.toFloat() * 2, compare );
                }
                else if( lcField.compare( "score", Qt::CaseInsensitive ) == 0 || lcField.compare( shortI18nForField( Meta::valScore ), Qt::CaseInsensitive ) == 0 )
                {
                    ADD_OR_EXCLUDE_NUMBER_FILTER( Meta::valScore, elem.text.toInt(), compare );
                }
                else if( lcField.compare( "playcount", Qt::CaseInsensitive ) == 0 || lcField.compare( shortI18nForField( Meta::valPlaycount ), Qt::CaseInsensitive ) == 0 )
                {
                    ADD_OR_EXCLUDE_NUMBER_FILTER( Meta::valPlaycount, elem.text.toInt(), compare );
                }
                else if( lcField.compare( "samplerate", Qt::CaseInsensitive ) == 0 || lcField.compare( shortI18nForField( Meta::valSamplerate ), Qt::CaseInsensitive ) == 0 )
                {
                    ADD_OR_EXCLUDE_NUMBER_FILTER( Meta::valSamplerate, elem.text.toInt(), compare );
                }
                else if( lcField.compare( "length", Qt::CaseInsensitive ) == 0 || lcField.compare( shortI18nForField( Meta::valLength ), Qt::CaseInsensitive ) == 0 )
                {
                    ADD_OR_EXCLUDE_NUMBER_FILTER( Meta::valLength, elem.text.toInt() * 1000, compare );
                }
                else if( lcField.compare( "filesize", Qt::CaseInsensitive ) == 0 || lcField.compare( shortI18nForField( Meta::valFilesize ), Qt::CaseInsensitive ) == 0 )
                {
                    bool doubleOk( false );
                    const double mbytes = elem.text.toDouble( &doubleOk ); // input in MBs
                    if( !doubleOk )
                    {
                        qm->endAndOr();
                        return;
                    }

                    /*
                     * A special case is made for Equals (e.g. filesize:100), which actually filters
                     * for anything beween 100 and 101MBs. Megabytes are used because for audio files
                     * they are the most reasonable units for the user to deal with.
                     */
                    const qreal bytes = mbytes * 1024.0 * 1024.0;
                    const qint64 mbFloor = qint64( qAbs(mbytes) );
                    switch( compare )
                    {
                    case Collections::QueryMaker::Equals:
                        qm->endAndOr();
                        qm->beginAnd();
                        ADD_OR_EXCLUDE_NUMBER_FILTER( Meta::valFilesize, mbFloor * 1024 * 1024, Collections::QueryMaker::GreaterThan );
                        ADD_OR_EXCLUDE_NUMBER_FILTER( Meta::valFilesize, (mbFloor + 1) * 1024 * 1024, Collections::QueryMaker::LessThan );
                        break;
                    case Collections::QueryMaker::GreaterThan:
                    case Collections::QueryMaker::LessThan:
                        ADD_OR_EXCLUDE_NUMBER_FILTER( Meta::valFilesize, bytes, compare );
                        break;
                    }
                }
                else if( lcField.compare( "format", Qt::CaseInsensitive ) == 0 || lcField.compare( shortI18nForField( Meta::valFormat ), Qt::CaseInsensitive ) == 0 )
                {
                    // NOTE: possible keywords that could be considered: codec, filetype, etc.
                    const QString &ftStr = elem.text;
                    Amarok::FileType ft = Amarok::FileTypeSupport::fileType(ftStr);

                    ADD_OR_EXCLUDE_NUMBER_FILTER( Meta::valFormat, int(ft), compare );
                }
                else if( lcField.compare( "discnumber", Qt::CaseInsensitive ) == 0 || lcField.compare( shortI18nForField( Meta::valDiscNr ), Qt::CaseInsensitive ) == 0 )
                {
                    ADD_OR_EXCLUDE_NUMBER_FILTER( Meta::valDiscNr, elem.text.toInt(), compare );
                }
                else if( lcField.compare( "tracknumber", Qt::CaseInsensitive ) == 0 || lcField.compare( shortI18nForField( Meta::valTrackNr ), Qt::CaseInsensitive ) == 0 )
                {
                    ADD_OR_EXCLUDE_NUMBER_FILTER( Meta::valTrackNr, elem.text.toInt(), compare );
                }
                else if( lcField.compare( "played", Qt::CaseInsensitive ) == 0 || lcField.compare( shortI18nForField( Meta::valLastPlayed ), Qt::CaseInsensitive ) == 0 )
                {
                    addDateFilter( Meta::valLastPlayed, compare, elem, qm );
                }
                else if( lcField.compare( "first", Qt::CaseInsensitive ) == 0 || lcField.compare( shortI18nForField( Meta::valFirstPlayed ), Qt::CaseInsensitive ) == 0 )
                {
                    addDateFilter( Meta::valFirstPlayed, compare, elem, qm );
                }
                else if( lcField.compare( "added", Qt::CaseInsensitive ) == 0 || lcField.compare( shortI18nForField( Meta::valCreateDate ), Qt::CaseInsensitive ) == 0 )
                {
                    addDateFilter( Meta::valCreateDate, compare, elem, qm );
                }
                else if( lcField.compare( "modified", Qt::CaseInsensitive ) == 0 || lcField.compare( shortI18nForField( Meta::valModified ), Qt::CaseInsensitive ) == 0 )
                {
                    addDateFilter( Meta::valModified, compare, elem, qm );
                }
            }
            qm->endAndOr();
#undef ADD_OR_EXCLUDE_FILTER
#undef ADD_OR_EXCLUDE_NUMBER_FILTER
        }
        qm->endAndOr();
    }
}

void
CollectionTreeItemModelBase::queryDone()
{
    DEBUG_BLOCK
    Collections::QueryMaker *qm = qobject_cast<Collections::QueryMaker*>( sender() );
    if( !qm )
        return;

    CollectionTreeItem* item = 0;
    if( d->childQueries.contains( qm ) )
        item = d->childQueries.take( qm );
    else if( d->compilationQueries.contains( qm ) )
        item = d->compilationQueries.take( qm );
    else if( d->noLabelsQueries.contains( qm ) )
        item = d->noLabelsQueries.take( qm );

    if( item )
        d->runningQueries.remove( item );

    //reset icon for this item
    if( item && item != m_rootItem )
    {
        emit dataChanged( createIndex(item->row(), 0, item), createIndex(item->row(), 0, item) );
    }

    //stop timer if there are no more animations active
    if( d->runningQueries.isEmpty() )
    {
        emit allQueriesFinished();
        m_timeLine->stop();
    }
    qm->deleteLater();
}

void
CollectionTreeItemModelBase::newResultReady(const QString & collectionId, Meta::DataList data)
{
    Q_UNUSED( collectionId )

    //if we are expanding an item, we'll find the sender in childQueries
    //otherwise we are filtering all collections
    Collections::QueryMaker *qm = qobject_cast<Collections::QueryMaker*>( sender() );
    if( !qm )
        return;

    if( d->childQueries.contains( qm ) )
        handleNormalQueryResult( qm, data );

    else if( d->compilationQueries.contains( qm ) )
        handleSpecialQueryResult( CollectionTreeItem::VariousArtist, qm, data );

    else if( d->noLabelsQueries.contains( qm ) )
        handleSpecialQueryResult( CollectionTreeItem::NoLabel, qm, data );
}

void
CollectionTreeItemModelBase::handleSpecialQueryResult( CollectionTreeItem::Type type, Collections::QueryMaker *qm, const Meta::DataList &dataList )
{
    DEBUG_BLOCK
    debug() << "Received special data: " << dataList.count();
    CollectionTreeItem *parent = 0;

    if( type == CollectionTreeItem::VariousArtist )
        parent = d->compilationQueries.value( qm );

    else if( type == CollectionTreeItem::NoLabel )
        parent = d->noLabelsQueries.value( qm );

    QModelIndex parentIndex;
    if( parent )
    {
        if (parent == m_rootItem ) // will never happen in CollectionTreeItemModel
            parentIndex = QModelIndex();
        else
            parentIndex = createIndex( parent->row(), 0, parent );

        //if the special query did not return a result we have to remove the
        //the special node itself
        if( dataList.isEmpty() )
        {
            for( int i = 0; i < parent->childCount(); i++ )
            {
                CollectionTreeItem *cti = parent->child( i );
                if( cti->type() == type )
                {
                    //found the special node
                    beginRemoveRows( parentIndex, cti->row(), cti->row() );
                    cti = 0; //will be deleted;
                    parent->removeChild( i );
                    endRemoveRows();
                    break;
                }
            }
            //we have removed the special node if it existed
            return;
        }

        CollectionTreeItem *specialNode = 0;
        if( parent->childCount() == 0 )
        {
            //we only insert the special node
            beginInsertRows( parentIndex, 0, 0 );
            specialNode = new CollectionTreeItem( type, dataList, parent, this );
            //set requiresUpdate, otherwise we will query for the children of specialNode again!
            specialNode->setRequiresUpdate( false );
            endInsertRows();
        }
        else
        {
            for( int i = 0; i < parent->childCount(); i++ )
            {
                CollectionTreeItem *cti = parent->child( i );
                if( cti->type() == type )
                {
                    //found the special node
                    specialNode = cti;
                    break;
                }
            }
            if( !specialNode )
            {
                //we only insert the special node
                beginInsertRows( parentIndex, 0, 0 );
                specialNode = new CollectionTreeItem( type, dataList, parent, this );
                //set requiresUpdate, otherwise we will query for the children of specialNode again!
                specialNode->setRequiresUpdate( false );
                endInsertRows();
            }
            else
            {
                //only call populateChildren for the special node if we have not
                //created it in this method call. The special node ctor takes care
                //of that itself
                populateChildren( dataList, specialNode, createIndex( specialNode->row(), 0, specialNode ) );
            }
            //populate children will call setRequiresUpdate on vaNode
            //but as the special query is based on specialNode's parent,
            //we have to call setRequiresUpdate on the parent too
            //yes, this will mean we will call setRequiresUpdate twice
            parent->setRequiresUpdate( false );

            for( int count = specialNode->childCount(), i = 0; i < count; ++i )
            {
                CollectionTreeItem *item = specialNode->child( i );
                if ( m_expandedItems.contains( item->data() ) ) //item will always be a data item
                {
                    listForLevel( item->level() + levelModifier(), item->queryMaker(), item );
                }
            }
        }

        //if the special node exists, check if it has to be expanded
        if( specialNode )
        {
            if( m_expandedSpecialNodes.contains( parent->parentCollection() ) )
            {
                emit expandIndex( createIndex( 0, 0, specialNode ) ); //we have just inserted the vaItem at row 0
            }
        }
    }
}

void
CollectionTreeItemModelBase::handleNormalQueryResult( Collections::QueryMaker *qm, const Meta::DataList &dataList )
{
    CollectionTreeItem *parent = d->childQueries.value( qm );
    QModelIndex parentIndex;
    if( parent ) {
        if( parent == m_rootItem ) // will never happen in CollectionTreeItemModel, but will happen in Single!
            parentIndex = QModelIndex();
        else
            parentIndex = createIndex( parent->row(), 0, parent );

        populateChildren( dataList, parent, parentIndex );

        if ( parent->isDataItem() )
        {
            if ( m_expandedItems.contains( parent->data() ) )
                emit expandIndex( parentIndex );
            else
                //simply insert the item, nothing will change if it is already in the set
                m_expandedItems.insert( parent->data() );
        }
        else
        {
            m_expandedCollections.insert( parent->parentCollection() );
        }
    }
}

void
CollectionTreeItemModelBase::populateChildren(const DataList & dataList, CollectionTreeItem * parent, const QModelIndex &parentIndex )
{
    //add new rows after existing ones here (which means all artists nodes
    //will be inserted after the "Various Artists" node)
    {
        //figure out which children of parent have to be removed,
        //which new children have to be added, and preemptively emit dataChanged for the rest
        //have to check how that influences performance...
        QHash<Meta::DataPtr, int> dataToIndex;
        QSet<Meta::DataPtr> childrenSet;
        foreach( CollectionTreeItem *child, parent->children() )
        {
            if( child->isVariousArtistItem() )
                continue;
            childrenSet.insert( child->data() );
            dataToIndex.insert( child->data(), child->row() );
        }
        QSet<Meta::DataPtr> dataSet = dataList.toSet();
        QSet<Meta::DataPtr> dataToBeAdded = dataSet - childrenSet;
        QSet<Meta::DataPtr> dataToBeRemoved = childrenSet - dataSet;

        QList<int> currentIndices;
        //first remove all rows that have to be removed
        //walking through the cildren in reverse order does not screw up the order
        for( int i = parent->childCount() - 1; i >= 0; i-- )
        {
            CollectionTreeItem *child = parent->child( i );
            bool toBeRemoved = child->isDataItem() && !child->isVariousArtistItem() && dataToBeRemoved.contains( child->data() ) ;
            if( toBeRemoved )
            {
                currentIndices.append( i );
            }
            //make sure we remove the rows if the first row (we are using reverse order) has to be removed too!
            if( ( !toBeRemoved || i == 0 ) && !currentIndices.isEmpty() )
            {
                //be careful in which order you insert the rows above!!
                beginRemoveRows( parentIndex, currentIndices.last(), currentIndices.first() );
                foreach( int i, currentIndices )
                {
                    parent->removeChild( i );
                }
                endRemoveRows();
                currentIndices.clear();
            }
        }
        //hopefully the view has figured out that we've removed rows yet!
        int lastRow = parent->childCount() - 1;
        if( lastRow >= 0 )
        {
            emit dataChanged( createIndex( 0, 0, parent->child( 0 ) ), createIndex( lastRow, 0, parent->child( lastRow ) ) );
        }
        //the remainging child items may be dirty, so refresh them
        foreach( CollectionTreeItem *item, parent->children() )
        {
            if( item->isDataItem() && item->data() && m_expandedItems.contains( item->data() ) )
                ensureChildrenLoaded( item );
        }
        //add the new rows
        if( !dataToBeAdded.isEmpty() )
        {
            //the above check ensures that Qt does not crash on beginInsertRows ( because lastRow+1 > lastRow+0)
            beginInsertRows( parentIndex, lastRow + 1, lastRow + dataToBeAdded.count() );
            foreach( Meta::DataPtr data, dataToBeAdded )
            {
                new CollectionTreeItem( data, parent, this );
            }
            endInsertRows();
        }
        parent->setRequiresUpdate( false );
    }
}

void
CollectionTreeItemModelBase::updateHeaderText()
{
    m_headerText.clear();
    for( int i=0; i< m_levelType.count(); ++i )
        m_headerText += nameForLevel( i ) + " / ";

    m_headerText.chop( 3 );
}

QString
CollectionTreeItemModelBase::nameForLevel(int level) const
{
    switch( m_levelType[level] )
    {
        case CategoryId::Album      : return AmarokConfig::showYears() ? i18n( "Year - Album" ) : i18n( "Album" );
        case CategoryId::Artist     : return i18n( "Artist" );
        case CategoryId::AlbumArtist: return i18n( "Album Artist" );
        case CategoryId::Composer   : return i18n( "Composer" );
        case CategoryId::Genre      : return i18n( "Genre" );
        case CategoryId::Year       : return i18n( "Year" );
        case CategoryId::Label      : return i18n( "Label" );

        default: return QString();
    }
}

void
CollectionTreeItemModelBase::handleCompilations( CollectionTreeItem *parent ) const
{
    DEBUG_BLOCK

    //this method will be called when we retrieve a list of artists from the database.
    //we have to query for all compilations, and then add a "Various Artists" node if at least
    //one compilation exists
    Collections::QueryMaker *qm = parent->queryMaker();
    qm->setAlbumQueryMode( Collections::QueryMaker::OnlyCompilations );
    qm->setQueryType( Collections::QueryMaker::Album );
    for( CollectionTreeItem *tmpItem = parent; tmpItem->parent(); tmpItem = tmpItem->parent() )
        tmpItem->addMatch( qm );

    addFilters( qm );
    qm->setReturnResultAsDataPtrs( true );
    connect( qm, SIGNAL( newResultReady( QString, Meta::DataList ) ), SLOT( newResultReady( QString, Meta::DataList ) ), Qt::QueuedConnection );
    connect( qm, SIGNAL( queryDone() ), SLOT( queryDone() ), Qt::QueuedConnection );
    d->compilationQueries.insert( qm, parent );
    d->runningQueries.insert( parent, qm );
    qm->run();
}

void
CollectionTreeItemModelBase::handleTracksWithoutLabels( Collections::QueryMaker::QueryType queryType, CollectionTreeItem *parent ) const
{
    DEBUG_BLOCK
    Collections::QueryMaker *qm = parent->queryMaker();
    qm->setQueryType( queryType );
    qm->setLabelQueryMode( Collections::QueryMaker::OnlyWithoutLabels );
    qm->setReturnResultAsDataPtrs( true );
    for( CollectionTreeItem *tmpItem = parent; tmpItem->parent(); tmpItem = tmpItem->parent() )
        tmpItem->addMatch( qm );

    addFilters( qm );
    connect( qm, SIGNAL( newResultReady( QString, Meta::DataList ) ), SLOT( newResultReady( QString, Meta::DataList ) ), Qt::QueuedConnection );
    connect( qm, SIGNAL( queryDone() ), SLOT( queryDone() ), Qt::QueuedConnection );
    d->noLabelsQueries.insert( qm, parent );
    d->runningQueries.insert( parent, qm );
    qm->run();
}

QDateTime
CollectionTreeItemModelBase::semanticDateTimeParser( const QString &text ) const
{
    /* TODO: semanticDateTimeParser: has potential to extend and form a class of its own */

    const QString lowerText = text.toLower();
    const QDateTime curTime = QDateTime::currentDateTime();
    QDateTime result;

    if( text.at(0).isLetter() )
    {
        if( ( lowerText.compare( "today" ) == 0 ) || ( lowerText.compare( i18n( "today" ) ) == 0 ) )
            result = curTime.addDays( -1 );
        else if( ( lowerText.compare( "last week" ) == 0 ) || ( lowerText.compare( i18n( "last week" ) ) == 0 ) )
            result = curTime.addDays( -7 );
        else if( ( lowerText.compare( "last month" ) == 0 ) || ( lowerText.compare( i18n( "last month" ) ) == 0 ) )
            result = curTime.addMonths( -1 );
        else if( ( lowerText.compare( "two months ago" ) == 0 ) || ( lowerText.compare( i18n( "two months ago" ) ) == 0 ) )
            result = curTime.addMonths( -2 );
        else if( ( lowerText.compare( "three months ago" ) == 0 ) || ( lowerText.compare( i18n( "three months ago" ) ) == 0 ) )
            result = curTime.addMonths( -3 );
    }
    else // first character is a number
    {
        // parse a "#m#d" (discoverability == 0, but without a GUI, how to do it?)
        int years = 0, months = 0, weeks = 0, days = 0, secs = 0;
        QString tmp;
        for( int i = 0; i < text.length(); i++ )
        {
            QChar c = text.at( i );
            if( c.isNumber() )
            {
                tmp += c;
            }
            else if( c == 'y' )
            {
                years = -tmp.toInt();
                tmp.clear();
            }
            else if( c == 'm' )
            {
                months = -tmp.toInt();
                tmp.clear();
            }
            else if( c == 'w' )
            {
                weeks = -tmp.toInt() * 7;
                tmp.clear();
            }
            else if( c == 'd' )
            {
                days = -tmp.toInt();
                break;
            }
            else if( c == 'h' )
            {
                secs = -tmp.toInt() * 60 * 60;
                break;
            }
            else if( c == 'M' )
            {
                secs = -tmp.toInt() * 60;
                break;
            }
            else if( c == 's' )
            {
                secs = -tmp.toInt();
                break;
            }
        }
        result = QDateTime::currentDateTime().addYears( years ).addMonths( months ).addDays( weeks ).addDays( days ).addSecs( secs );
    }
    return result;
}

void CollectionTreeItemModelBase::startAnimationTick()
{
    //start animation
    if( ( m_timeLine->state() != QTimeLine::Running ) && !d->runningQueries.isEmpty() )
        m_timeLine->start();
}

void CollectionTreeItemModelBase::loadingAnimationTick()
{
    if ( m_animFrame == 0 )
        m_currentAnimPixmap = m_loading2;
    else
        m_currentAnimPixmap = m_loading1;

    m_animFrame = 1 - m_animFrame;

    //trigger an update of all items being populated at the moment;

    QList< CollectionTreeItem * > items = d->runningQueries.keys();
    foreach ( CollectionTreeItem* item, items  )
    {
        if( item == m_rootItem )
            continue;
        emit dataChanged ( createIndex(item->row(), 0, item), createIndex(item->row(), 0, item) );
    }
}

void
CollectionTreeItemModelBase::setCurrentFilter( const QString &filter )
{
    m_currentFilter = filter;
}

void
CollectionTreeItemModelBase::slotFilter()
{
    filterChildren();
    if ( !m_expandedCollections.isEmpty() )
    {
        foreach( Collections::Collection *expanded, m_expandedCollections )
        {
            CollectionTreeItem *expandedItem = d->collections.value( expanded->collectionId() ).second;
            if( expandedItem )
                emit expandIndex( createIndex( expandedItem->row(), 0, expandedItem ) );
        }
    }
}

void
CollectionTreeItemModelBase::slotCollapsed( const QModelIndex &index )
{
    if ( index.isValid() )      //probably unnecessary, but let's be safe
    {
        CollectionTreeItem *item = static_cast<CollectionTreeItem*>( index.internalPointer() );

        switch( item->type() )
        {
        case CollectionTreeItem::Root:
            break; // nothing to do

        case CollectionTreeItem::Collection:
            m_expandedCollections.remove( item->parentCollection() );
            break;

        case CollectionTreeItem::VariousArtist:
        case CollectionTreeItem::NoLabel:
            m_expandedSpecialNodes.remove( item->parentCollection() );
            break;
        case CollectionTreeItem::Data:
            m_expandedItems.remove( item->data() );
            break;
        }
    }
}

void
CollectionTreeItemModelBase::slotExpanded( const QModelIndex &index )
{
    if( index.isValid() )
    {
        CollectionTreeItem *item = static_cast<CollectionTreeItem*>( index.internalPointer() );
        //we are really only interested in the special nodes here.
        //we have to remember whether the user expanded a various artists/no labels node or not.
        //otherwise we won't be able to automatically expand the special node after filtering again
        //there is exactly one special node per type per collection, so use the collection to store that information
        switch( item->type() )
        {
        case CollectionTreeItem::VariousArtist:
        case CollectionTreeItem::NoLabel:
            m_expandedSpecialNodes.insert( item->parentCollection() );
            break;
        default:
            break;
        }
    }
}

void CollectionTreeItemModelBase::update()
{
    reset();
}

void CollectionTreeItemModelBase::updateRowHeight()
{
    QFont font;
    QFontMetrics fm( font );
    d->rowHeight = fm.height() + 4;
}

void CollectionTreeItemModelBase::markSubTreeAsDirty( CollectionTreeItem *item )
{
    //tracks are the leaves so they are never dirty
    if( !item->isTrackItem() )
        item->setRequiresUpdate( true );
    for( int i = 0; i < item->childCount(); i++ )
    {
        markSubTreeAsDirty( item->child( i ) );
    }
}

void CollectionTreeItemModelBase::itemAboutToBeDeleted( CollectionTreeItem *item )
{
    if( !d->runningQueries.contains( item ) )
        return;
    //replace this hack with QWeakPointer as soon as we depend on Qt 4.6
    Collections::QueryMaker *qm = d->runningQueries.take( item );
    if( qm )
    {

        d->childQueries.remove( qm );
        d->compilationQueries.remove( qm );
        d->noLabelsQueries.remove( qm );
        //we still need to disconnect the qm below
    }
    if( qm )
    {
        //Disconnect all signals from the QueryMaker so we do not get notified about the results
        qm->disconnect();
        //Nuke it
        qm->deleteLater();
    }
}

int
CollectionTreeItemModelBase::levelCategory( const int level ) const
{
    if( level >= 0 && level + levelModifier() < m_levelType.count() )
        return m_levelType[ level + levelModifier() ];

    return CategoryId::None;
}

#include "CollectionTreeItemModelBase.moc"

