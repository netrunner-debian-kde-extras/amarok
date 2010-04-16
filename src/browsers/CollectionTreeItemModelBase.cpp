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

#include <KIcon>
#include <KIconLoader>
#include <KLocale>
#include <KStandardDirs>
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
    connect( m_timeLine, SIGNAL( frameChanged( int ) ), this, SLOT( loadingAnimationTick() ) );
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
        if( tracks.size() > 0 )
        {
            foreach( Meta::TrackPtr track, tracks )
            {
                Capabilities::EditCapability *ec = track->create<Capabilities::EditCapability>();
                if( ec )
                    ec->setAlbum( value.toString() );
                delete ec;
            }
            emit dataChanged( index, index );
            return true;
        }
    }
    else if( Meta::ArtistPtr artist = Meta::ArtistPtr::dynamicCast( data ) )
    {
        Meta::TrackList tracks = artist->tracks();
        if( tracks.size() > 0 )
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
        if( tracks.size() > 0 )
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
        if( tracks.size() > 0 )
        {
            foreach( Meta::TrackPtr track, tracks )
            {
                Capabilities::EditCapability *ec = track->create<Capabilities::EditCapability>();
                if( ec )
                    ec->setYear( value.toString() );
                delete ec;
            }
            emit dataChanged( index, index );
            return true;
        }
    }
    else if( Meta::ComposerPtr composer = Meta::ComposerPtr::dynamicCast( data ) )
    {
        Meta::TrackList tracks = composer->tracks();
        if( tracks.size() > 0 )
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
                    qm->addMatch( tmpItem->data() );
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
    if ( item->requiresUpdate() && !d->m_runningQueries.contains( item ) )
    {
        listForLevel( item->level() + levelModifier(), item->queryMaker(), item );
    }
}

QPixmap
CollectionTreeItemModelBase::iconForLevel(int level) const
{
    QString icon;
    switch( m_levelType[level] )
    {
        case CategoryId::Album :
//             icon = "view-media-album-amarok"; // Doesn't exist..
            icon = "media-optical-amarok";
            break;
        case CategoryId::Artist :
            icon = "view-media-artist-amarok";
            break;
        case CategoryId::Composer :
            icon = "view-media-artist-amarok";
            break;
        case CategoryId::Genre :
            icon = "favorite-genres-amarok";
            break;
        case CategoryId::Year :
            icon = "clock";
            break;
        case CategoryId::Label :
            icon = "label"; //FIXME
            break;
    }
    return KIconLoader::global()->loadIcon( icon, KIconLoader::Toolbar, KIconLoader::SizeSmall );
}

void CollectionTreeItemModelBase::listForLevel(int level, Collections::QueryMaker * qm, CollectionTreeItem * parent)
{
    DEBUG_BLOCK
    if ( qm && parent )
    {
        //this check should not hurt anyone... needs to check if single... needs it
        if( d->m_runningQueries.contains( parent ) )
            return;

        if ( level > m_levelType.count() )
            return;

        if( parent->isVariousArtistItem() || parent->isNoLabelItem() )
            return;

        if ( level == m_levelType.count() )
            qm->setQueryType( Collections::QueryMaker::Track );
        else
        {
            switch( m_levelType[level] )
            {
                case CategoryId::Album :
                    qm->setQueryType( Collections::QueryMaker::Album );
                    //restrict query to normal albums if the previous level
                    //was the artist category. in that case we handle compilations below
                    if( level > 0 && m_levelType[level-1] == CategoryId::Artist && !parent->isVariousArtistItem() )
                    {
                        qm->setAlbumQueryMode( Collections::QueryMaker::OnlyNormalAlbums );
                    }
                    break;
                case CategoryId::Artist :
                    qm->setQueryType( Collections::QueryMaker::Artist );
                    //handle compilations only if the next level ist CategoryId::Album
                    if( level + 1 < m_levelType.count() && m_levelType[level+1] == CategoryId::Album )
                    {
                        handleCompilations( parent );
                        qm->setAlbumQueryMode( Collections::QueryMaker::OnlyNormalAlbums );
                    }
                    break;
                case CategoryId::Label:
                    qm->setQueryType( Collections::QueryMaker::Label );
                    //we do not have to restrict to OnlqWithLabels here, as these obviously cannot be returned when querying for labels
                    //but we need to handle tracks without labels
                    Collections::QueryMaker::QueryType nextLevel;
                    if( level +1 == m_levelType.count() )
                    {
                        nextLevel = Collections::QueryMaker::Track;
                    }
                    else
                    {
                        nextLevel = mapCategoryToQueryType( m_levelType [ level + 1 ] );
                    }
                    handleTracksWithoutLabels( nextLevel, parent );
                    break;

                default : //TODO handle error condition. return tracks?
                    qm->setQueryType( mapCategoryToQueryType( m_levelType[ level ] ) );
                    break;
            }
        }
        CollectionTreeItem *tmpItem = parent;
        while( tmpItem->isDataItem() )
        {
            //ignore Various artists node (which will not have a data pointer)
            if( tmpItem->isVariousArtistItem() )
            {
                qm->setAlbumQueryMode( Collections::QueryMaker::OnlyCompilations );
            }
            else
                qm->addMatch( tmpItem->data() );

            tmpItem = tmpItem->parent();
        }
        addFilters( qm );
        qm->setReturnResultAsDataPtrs( true );
        connect( qm, SIGNAL( newResultReady( QString, Meta::DataList ) ), SLOT( newResultReady( QString, Meta::DataList ) ), Qt::QueuedConnection );
        connect( qm, SIGNAL( queryDone() ), SLOT( queryDone() ), Qt::QueuedConnection );
        d->m_childQueries.insert( qm, parent );
        d->m_runningQueries.insert( parent, qm );
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

void
CollectionTreeItemModelBase::addFilters( Collections::QueryMaker * qm ) const
{
    int validFilters = qm->validFilterMask();

    ParsedExpression parsed = ExpressionParser::parse ( m_currentFilter );
    foreach( const or_list &orList, parsed )
    {
        qm->beginOr();

        foreach ( const expression_element &elem, orList )
        {
#define ADD_OR_EXCLUDE_FILTER( VALUE, FILTER, MATCHBEGIN, MATCHEND ) \
            if( elem.negate ) \
                qm->excludeFilter( VALUE, FILTER, MATCHBEGIN, MATCHEND ); \
            else \
                qm->addFilter( VALUE, FILTER, MATCHBEGIN, MATCHEND );
#define ADD_OR_EXCLUDE_NUMBER_FILTER( VALUE, FILTER, COMPARE ) \
            if( elem.negate ) \
                qm->excludeNumberFilter( VALUE, FILTER, COMPARE ); \
            else \
                qm->addNumberFilter( VALUE, FILTER, COMPARE );
            if( elem.negate )
                qm->beginAnd();
            else
                qm->beginOr();

            if ( elem.field.isEmpty() )
            {
                foreach ( int level, m_levelType )
                {
                    qint64 value;
                    switch ( level )
                    {
                        case CategoryId::Album:
                            if ( ( validFilters & Collections::QueryMaker::AlbumFilter ) == 0 ) continue;
                            value = Meta::valAlbum;
                            break;
                        case CategoryId::Artist:
                            if ( ( validFilters & Collections::QueryMaker::ArtistFilter ) == 0 ) continue;
                            value = Meta::valArtist;
                            break;
                        case CategoryId::Composer:
                            if ( ( validFilters & Collections::QueryMaker::ComposerFilter ) == 0 ) continue;
                            value = Meta::valComposer;
                            break;
                        case CategoryId::Genre:
                            if ( ( validFilters & Collections::QueryMaker::GenreFilter ) == 0 ) continue;
                            value = Meta::valGenre;
                            break;
                        case CategoryId::Year:
                            if ( ( validFilters & Collections::QueryMaker::YearFilter ) == 0 ) continue;
                            value = Meta::valYear;
                            break;
                        case CategoryId::Label:
                            value = Meta::valLabel;
                            break;
                        default:
                            value = -1;
                            break;
                    }
                    qm->beginOr();
                    ADD_OR_EXCLUDE_FILTER( value, elem.text, false, false );
                    qm->endAndOr();
                }

                //always add track filter ( if supported..)
                if ( ( validFilters & Collections::QueryMaker::TitleFilter ) != 0 ) {
                    qm->beginOr();
                    ADD_OR_EXCLUDE_FILTER( Meta::valTitle, elem.text, false, false ); //always filter for track title too
                    qm->endAndOr();
                }
                if ( ( validFilters & Collections::QueryMaker::UrlFilter ) != 0 ) {
                    qm->beginOr();
                    ADD_OR_EXCLUDE_FILTER( Meta::valUrl, elem.text, false, false ); //always filter for track URL
                    qm->endAndOr();
                }

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

                if ( lcField.compare( "album", Qt::CaseInsensitive ) == 0 || lcField.compare( i18n( "album" ), Qt::CaseInsensitive ) == 0 )
                {
                    if ( ( validFilters & Collections::QueryMaker::AlbumFilter ) == 0 ) continue;
                    ADD_OR_EXCLUDE_FILTER( Meta::valAlbum, elem.text, false, false );
                }
                else if ( lcField.compare( "artist", Qt::CaseInsensitive ) == 0 || lcField.compare( i18n( "artist" ), Qt::CaseInsensitive ) == 0 )
                {
                    if ( ( validFilters & Collections::QueryMaker::ArtistFilter ) == 0 ) continue;
                    ADD_OR_EXCLUDE_FILTER( Meta::valArtist, elem.text, false, false );
                }
                else if ( lcField.compare( "genre", Qt::CaseInsensitive ) == 0 || lcField.compare( i18n( "genre" ), Qt::CaseInsensitive ) == 0)
                {
                    if ( ( validFilters & Collections::QueryMaker::GenreFilter ) == 0 ) continue;
                    ADD_OR_EXCLUDE_FILTER( Meta::valGenre, elem.text, false, false );
                }
                else if ( lcField.compare( "title", Qt::CaseInsensitive ) == 0 || lcField.compare( i18n( "title" ), Qt::CaseInsensitive ) == 0 )
                {
                    if ( ( validFilters & Collections::QueryMaker::TitleFilter ) == 0 ) continue;
                    ADD_OR_EXCLUDE_FILTER( Meta::valTitle, elem.text, false, false );
                }
                else if ( lcField.compare( "composer", Qt::CaseInsensitive ) == 0 || lcField.compare( i18n( "composer" ), Qt::CaseInsensitive ) == 0 )
                {
                    if ( ( validFilters & Collections::QueryMaker::ComposerFilter ) == 0 ) continue;
                    ADD_OR_EXCLUDE_FILTER( Meta::valComposer, elem.text, false, false );
                }
                else if( lcField.compare( "label", Qt::CaseInsensitive ) == 0 || lcField.compare( i18n( "label" ), Qt::CaseInsensitive ) == 0 )
                {
                    ADD_OR_EXCLUDE_FILTER( Meta::valLabel, elem.text, false, false );
                }
                else if ( lcField.compare( "year", Qt::CaseInsensitive ) == 0 || lcField.compare( i18n( "year" ), Qt::CaseInsensitive ) == 0)
                {
                    if ( ( validFilters & Collections::QueryMaker::YearFilter ) == 0 ) continue;
                    ADD_OR_EXCLUDE_NUMBER_FILTER( Meta::valYear, elem.text.toInt(), compare );
                }
                else if ( lcField.compare( "bpm", Qt::CaseInsensitive ) == 0 || lcField.compare( i18n( "bpm" ), Qt::CaseInsensitive ) == 0)
                {
                    ADD_OR_EXCLUDE_NUMBER_FILTER( Meta::valBpm, elem.text.toInt(), compare );
                }
                else if( lcField.compare( "comment", Qt::CaseInsensitive ) == 0 || lcField.compare( i18n( "comment" ), Qt::CaseInsensitive ) == 0 )
                {
                    ADD_OR_EXCLUDE_FILTER( Meta::valYear, elem.text, false, false );
                }
                else if( lcField.compare( "bitrate", Qt::CaseInsensitive ) == 0 || lcField.compare( i18n( "bitrate" ), Qt::CaseInsensitive ) == 0 )
                {
                    ADD_OR_EXCLUDE_NUMBER_FILTER( Meta::valBitrate, elem.text.toInt(), compare );
                }
                else if( lcField.compare( "rating", Qt::CaseInsensitive ) == 0 || lcField.compare( i18n( "rating" ), Qt::CaseInsensitive ) == 0 )
                {
                    ADD_OR_EXCLUDE_NUMBER_FILTER( Meta::valRating, elem.text.toInt(), compare );
                }
                else if( lcField.compare( "score", Qt::CaseInsensitive ) == 0 || lcField.compare( i18n( "score" ), Qt::CaseInsensitive ) == 0 )
                {
                    ADD_OR_EXCLUDE_NUMBER_FILTER( Meta::valScore, elem.text.toInt(), compare );
                }
                else if( lcField.compare( "playcount", Qt::CaseInsensitive ) == 0 || lcField.compare( i18n( "playcount" ), Qt::CaseInsensitive ) == 0 )
                {
                    ADD_OR_EXCLUDE_NUMBER_FILTER( Meta::valPlaycount, elem.text.toInt(), compare );
                }
                else if( lcField.compare( "samplerate", Qt::CaseInsensitive ) == 0 || lcField.compare( i18n( "samplerate" ), Qt::CaseInsensitive ) == 0 )
                {
                    ADD_OR_EXCLUDE_NUMBER_FILTER( Meta::valSamplerate, elem.text.toInt(), compare );
                }
                else if( lcField.compare( "length", Qt::CaseInsensitive ) == 0 || lcField.compare( i18n( "length" ), Qt::CaseInsensitive ) == 0 )
                {
                    ADD_OR_EXCLUDE_NUMBER_FILTER( Meta::valLength, elem.text.toInt(), compare );
                }
                else if( lcField.compare( "discnumber", Qt::CaseInsensitive ) == 0 || lcField.compare( i18n( "discnumber" ), Qt::CaseInsensitive ) == 0 )
                {
                    ADD_OR_EXCLUDE_NUMBER_FILTER( Meta::valDiscNr, elem.text.toInt(), compare );
                }
                else if( lcField.compare( "tracknumber", Qt::CaseInsensitive ) == 0 || lcField.compare( i18n( "tracknumber" ), Qt::CaseInsensitive ) == 0 )
                {
                    ADD_OR_EXCLUDE_NUMBER_FILTER( Meta::valTrackNr, elem.text.toInt(), compare );
                }
                else if( lcField.compare( "added", Qt::CaseInsensitive ) == 0 || lcField.compare( i18n( "added" ), Qt::CaseInsensitive ) == 0 )
                {
                    if( compare == Collections::QueryMaker::Equals ) // just do some basic string matching
                    {
                        QDateTime curTime = QDateTime::currentDateTime();
                        uint dateCutOff = 0;
                        if( ( elem.text.compare( "today", Qt::CaseInsensitive ) == 0 ) || ( elem.text.compare( i18n( "today" ), Qt::CaseInsensitive ) == 0 ) )
                            dateCutOff = curTime.addDays( -1 ).toTime_t();
                        else if( ( elem.text.compare( "last week", Qt::CaseInsensitive ) == 0 ) || ( elem.text.compare( i18n( "last week" ), Qt::CaseInsensitive ) == 0 ) )
                            dateCutOff = curTime.addDays( -7 ).toTime_t();
                        else if( ( elem.text.compare( "last month", Qt::CaseInsensitive ) == 0 ) || ( elem.text.compare( i18n( "last month" ), Qt::CaseInsensitive ) == 0 ) )
                            dateCutOff = curTime.addMonths( -1 ).toTime_t();
                        else if( ( elem.text.compare( "two months ago", Qt::CaseInsensitive ) == 0 ) || ( elem.text.compare( i18n( "two months ago" ), Qt::CaseInsensitive ) == 0 ) )
                            dateCutOff = curTime.addMonths( -2 ).toTime_t();
                        else if( ( elem.text.compare( "three months ago", Qt::CaseInsensitive ) == 0 ) || ( elem.text.compare( i18n( "three months ago" ), Qt::CaseInsensitive ) == 0 ) )
                            dateCutOff = curTime.addMonths( -3 ).toTime_t();

                        if( dateCutOff > 0 )
                        {
                            ADD_OR_EXCLUDE_NUMBER_FILTER( Meta::valCreateDate, dateCutOff, Collections::QueryMaker::GreaterThan );
                        }
                    }
                    else if( compare == Collections::QueryMaker::LessThan ) // parse a "#m#d" (discoverability == 0, but without a GUI, how to do it?)
                    {
                        int months = 0, weeks = 0, days = 0;
                        QString tmp;
                        for( int i = 0; i < elem.text.length(); i++ )
                        {
                            QChar c = elem.text.at( i );
                            if( c.isNumber() )
                                tmp += c;
                            else if( c == 'm' )
                            {
                                months = 0 - tmp.toInt();
                                tmp.clear();
                            } else if( c == 'w' )
                            {
                                weeks = 0 - 7 * tmp.toInt();
                                tmp.clear();
                            } else if( c == 'd' )
                            {
                                days = 0 - tmp.toInt();
                                break;
                            }
                        }
                        ADD_OR_EXCLUDE_NUMBER_FILTER( Meta::valCreateDate, QDateTime::currentDateTime().addMonths( months ).addDays( weeks ).addDays( days ).toTime_t(), Collections::QueryMaker::GreaterThan );
                    }
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
    Collections::QueryMaker *qm = qobject_cast<Collections::QueryMaker*>( sender() );
    if( !qm )
        return;

    CollectionTreeItem* item = d->m_childQueries.contains( qm ) ? d->m_childQueries.take( qm ) : d->m_compilationQueries.take( qm );

    d->m_runningQueries.remove( item );

    //reset icon for this item
    if( item && item != m_rootItem )
    {
        emit dataChanged( createIndex(item->row(), 0, item), createIndex(item->row(), 0, item) );
        emit queryFinished();
    }

    //stop timer if there are no more animations active
    if( d->m_runningQueries.count() == 0 )
        m_timeLine->stop();
}

void
CollectionTreeItemModelBase::newResultReady(const QString & collectionId, Meta::DataList data)
{
    Q_UNUSED( collectionId )

    //if we are expanding an item, we'll find the sender in m_childQueries
    //otherwise we are filtering all collections
    Collections::QueryMaker *qm = qobject_cast<Collections::QueryMaker*>( sender() );
    if( !qm )
        return;

    if( d->m_childQueries.contains( qm ) )
    {
        handleNormalQueryResult( qm, data );
    }
    else if( d->m_compilationQueries.contains( qm ) )
    {
        handleSpecialQueryResult( CollectionTreeItem::VariousArtist, qm, data );
    }
    else if( d->noLabelsQueries.contains( qm ) )
    {
        handleSpecialQueryResult( CollectionTreeItem::NoLabel, qm, data );
    }
}

void
CollectionTreeItemModelBase::handleSpecialQueryResult( CollectionTreeItem::Type type, Collections::QueryMaker *qm, const Meta::DataList &dataList )
{
    DEBUG_BLOCK
    debug() << "Received special data: " << dataList.count();
    CollectionTreeItem *parent = 0;
    if( type == CollectionTreeItem::VariousArtist )
        parent = d->m_compilationQueries.value( qm );
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
        if( specialNode)
        {
            CollectionTreeItem *tmp = parent;
            while( tmp->isDataItem() )
                tmp = tmp->parent();

            if( type == CollectionTreeItem::VariousArtist )
            {
                if( m_expandedVariousArtistsNodes.contains( tmp->parentCollection() ) )
                {
                    emit expandIndex( createIndex( 0, 0, specialNode ) ); //we have just inserted the vaItem at row 0
                }
            }
            else if( type == CollectionTreeItem::NoLabel )
            {
                if( m_expandedNoLabelsNodes.contains( tmp->parentCollection() ) )
                {
                    emit expandIndex( createIndex( 0, 0, specialNode ) ); //we have just inserted the special node at row 0
                }
            }
        }
    }
}

void
CollectionTreeItemModelBase::handleNormalQueryResult( Collections::QueryMaker *qm, const Meta::DataList &dataList )
{
    CollectionTreeItem *parent = d->m_childQueries.value( qm );
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
    //this method will be called when we retrieve a list of artists from the database.
    //we have to query for all compilations, and then add a "Various Artists" node if at least
    //one compilation exists
    Collections::QueryMaker *qm = parent->queryMaker();
    qm->setAlbumQueryMode( Collections::QueryMaker::OnlyCompilations );
    qm->setQueryType( Collections::QueryMaker::Album );
    CollectionTreeItem *tmpItem = parent;
    while( tmpItem->isDataItem()  )
    {
        //ignore Various artists node (which will not have a data pointer)
        if( !tmpItem->isVariousArtistItem() && !tmpItem->isNoLabelItem() )
            qm->addMatch( tmpItem->data() );
        tmpItem = tmpItem->parent();
    }
    addFilters( qm );
    qm->setReturnResultAsDataPtrs( true );
    connect( qm, SIGNAL( newResultReady( QString, Meta::DataList ) ), SLOT( newResultReady( QString, Meta::DataList ) ), Qt::QueuedConnection );
    connect( qm, SIGNAL( queryDone() ), SLOT( queryDone() ), Qt::QueuedConnection );
    d->m_compilationQueries.insert( qm, parent );
    d->m_runningQueries.insert( parent, qm );
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
    CollectionTreeItem *tmpItem = parent;
    while( tmpItem->isDataItem() )
    {
        //ignore special nodes that do not have a data pointer
        if( !tmpItem->isVariousArtistItem() && !tmpItem->isNoLabelItem() )
        {
            qm->addMatch( tmpItem->data() );
        }
        tmpItem = tmpItem->parent();
    }
    addFilters( qm );
    connect( qm, SIGNAL( newResultReady( QString, Meta::DataList ) ), SLOT( newResultReady( QString, Meta::DataList ) ), Qt::QueuedConnection );
    connect( qm, SIGNAL( queryDone() ), SLOT( queryDone() ), Qt::QueuedConnection );
    d->noLabelsQueries.insert( qm, parent );
    d->m_runningQueries.insert( parent, qm );
    qm->run();
}

void CollectionTreeItemModelBase::startAnimationTick()
{
    //start animation
    if( ( m_timeLine->state() != QTimeLine::Running ) && !d->m_runningQueries.isEmpty() )
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

    QList< CollectionTreeItem * > items = d->m_runningQueries.keys();
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
            CollectionTreeItem *expandedItem = d->m_collections.value( expanded->collectionId() ).second;
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
        if ( item->isDataItem() && !item->isVariousArtistItem() )
        {
            m_expandedItems.remove( item->data() );
        }
        else if( item->isVariousArtistItem() ) //Various artists nodes
        {
            CollectionTreeItem *tmp = item->parent();
            while( tmp->isDataItem() )
                tmp = tmp->parent();

            m_expandedVariousArtistsNodes.remove( tmp->parentCollection() );
        }
        else if( item->isNoLabelItem() )
        {
            CollectionTreeItem *tmp = item->parent();
            while( tmp->isDataItem() )
                tmp = tmp->parent();

            m_expandedNoLabelsNodes.remove( tmp->parentCollection() );
        }
        else
        {
            m_expandedCollections.remove( item->parentCollection() );
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
        if( item->isVariousArtistItem() )
        {
            CollectionTreeItem *tmp = item->parent();
            while( tmp->isDataItem() )
                tmp = tmp->parent();

            m_expandedVariousArtistsNodes.insert( tmp->parentCollection() );
        }
        else if( item->isNoLabelItem() )
        {
            CollectionTreeItem *tmp = item->parent();
            while( tmp->isDataItem() )
                tmp = tmp->parent();

            m_expandedNoLabelsNodes.insert( tmp->parentCollection() );
        }
    }
}

void CollectionTreeItemModelBase::update()
{
    reset();
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
    if( !d->m_runningQueries.contains( item ) )
        return;
    //replace this hack with QWeakPointer as soon as we depend on Qt 4.6
    Collections::QueryMaker *qm = d->m_runningQueries.take( item );
    if( qm )
    {

        d->m_childQueries.remove( qm );
        d->m_compilationQueries.remove( qm );
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

#include "CollectionTreeItemModelBase.moc"

