/****************************************************************************************
 * Copyright (c) 2010 Maximilian Kossick <maximilian.kossick@googlemail.com>            *
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

#include "CollectionLocationDelegateImpl.h"

#include "core/interfaces/Logger.h"
#include "core/collections/CollectionLocation.h"
#include "core/support/Components.h"

#include <KLocale>
#include <KMessageBox>

using namespace Collections;

bool
CollectionLocationDelegateImpl::reallyDelete( CollectionLocation *loc, const Meta::TrackList &tracks ) const
{
    Q_UNUSED( loc );

    QStringList files;
    foreach( Meta::TrackPtr track, tracks )
        files << track->prettyUrl();

    // NOTE: taken from SqlCollection
    // TODO put the delete confirmation code somewhere else?
    const QString text( i18ncp( "@info", "Do you really want to delete this track? It will be removed from disk as well as your collection.",
                                "Do you really want to delete these %1 tracks? They will be removed from disk as well as your collection.", tracks.count() ) );
    const bool del = KMessageBox::warningContinueCancelList(0,
                                                     text,
                                                     files,
                                                     i18n("Delete Files"),
                                                     KStandardGuiItem::del() ) == KMessageBox::Continue;

    return del;
}

bool
CollectionLocationDelegateImpl::reallyTrash( CollectionLocation *loc, const Meta::TrackList &tracks ) const
{
    Q_UNUSED( loc );

    QStringList files;
    foreach( Meta::TrackPtr track, tracks )
        files << track->prettyUrl();

    const QString text( i18ncp( "@info",
                                "Do you really want to move this track to the trash? "
                                "It will be removed from disk as well as your collection.",
                                "Do you really want to move these %1 tracks to the trash? "
                                "They will be removed from disk as well as your collection.",
                                tracks.count() ) );
    const bool rm = KMessageBox::warningContinueCancelList(
                                0,
                                text,
                                files,
                                i18nc( "@title:window", "Confirm Move to Trash" ),
                                KStandardGuiItem::remove() ) == KMessageBox::Continue;
    return rm;
}

bool CollectionLocationDelegateImpl::reallyMove(CollectionLocation* loc, const Meta::TrackList& tracks) const
{
    Q_UNUSED( loc )
    QStringList files;
    foreach( Meta::TrackPtr track, tracks )
        files << track->prettyUrl();
    
    const QString text( i18ncp( "@info", "Do you really want to move this track? It will be renamed and the original deleted.",
                                "Do you really want to move these %1 tracks? They will be renamed and the originals deleted.", tracks.count() ) );
    const bool del = KMessageBox::warningContinueCancelList(0,
                                                            text,
                                                            files,
                                                            i18n("Move Files") ) == KMessageBox::Continue;
    return del;
}

void CollectionLocationDelegateImpl::errorDeleting( CollectionLocation* loc, const Meta::TrackList& tracks ) const
{
    Q_UNUSED( loc );
    QStringList files;
    foreach( Meta::TrackPtr track, tracks )
        files << track->prettyUrl();

    const QString text( i18ncp( "@info", "There was a problem and this track could not be removed. Make sure the directory is writeable.",
                                "There was a problem and %1 tracks could not be removed. Make sure the directory is writeable.", files.count() ) );
                                KMessageBox::informationList(0,
                                                             text,
                                                             files,
                                                             i18n("Unable to be removed tracks") );
}

void CollectionLocationDelegateImpl::notWriteable(CollectionLocation* loc) const
{
    Q_UNUSED( loc )
    Amarok::Components::logger()->longMessage(
            i18n( "The collection does not have enough free space available or is not writeable." ),
            Amarok::Logger::Error );
}

bool CollectionLocationDelegateImpl::deleteEmptyDirs( CollectionLocation* loc ) const
{
    const QString text( i18n( "Do you want to remove empty folders?" ) );
    const QString caption( i18n( "Remove empty folders?" ) );
    const int result = KMessageBox::questionYesNo(0,
                               text,
                               caption,
                               KStandardGuiItem::yes(),
                               KStandardGuiItem::no(),
                               QString( "collectionlocation_" + loc->prettyLocation() ) );
    return result == KMessageBox::Yes ? true : false;
}

