/****************************************************************************************
 * Copyright (c) 2008 Daniel Winter <dw@danielwinter.de>                                *
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

#ifndef NEPOMUKARTIST_H
#define NEPOMUKARTIST_H

#include "NepomukCollection.h"

#include "core/meta/Meta.h"

class NepomukCollection;

namespace Meta
{

class NepomukArtist : public Artist
{
    public:
        NepomukArtist( NepomukCollection *collection, const QString &name );
        virtual ~NepomukArtist() {};

        virtual QString name() const;

        virtual TrackList tracks();

        virtual AlbumList albums();

        // for plugin internal use only

        void emptyCache();

    private:
        NepomukCollection *m_collection;
        QString m_name;
        AlbumList m_albums;
        bool m_albumsLoaded;
};

}
#endif /*NEPOMUKARTIST_H*/
