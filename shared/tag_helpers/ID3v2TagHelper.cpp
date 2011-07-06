/****************************************************************************************
 * Copyright    (C) 2003-2005 Max Howell <max.howell@methylblue.com>                    *
 *              (C) 2003-2010 Mark Kretschmann <kretschmann@kde.org>                    *
 *              (C) 2005-2007 Alexandre Oliveira <aleprj@gmail.com>                     *
 *              (C) 2008 Dan Meltzer <parallelgrapefruit@gmail.com>                     *
 *              (C) 2008-2009 Jeff Mitchell <mitchell@kde.org>                          *
 *              (C) 2010 Ralf Engels <ralf-engels@gmx.de>                               *
 *              (c) 2010 Sergey Ivanov <123kash@gmail.com>                              *
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

#include "ID3v2TagHelper.h"

#include <taglib/attachedpictureframe.h>
#include <taglib/popularimeterframe.h>
#include <taglib/textidentificationframe.h>
#include <taglib/uniquefileidentifierframe.h>

#ifndef UTILITIES_BUILD
    #include <QBuffer>
#endif  //UTILITIES_BUILD

#include "StringHelper.h"

const TagLib::ByteVector TXXX_Frame = "TXXX";
const TagLib::ByteVector POPM_Frame = "POPM";

using namespace Meta::Tag;

ID3v2TagHelper::ID3v2TagHelper( TagLib::Tag *tag, TagLib::ID3v2::Tag *id3v2Tag, Amarok::FileType fileType )
              : TagHelper( tag, fileType )
              , m_tag( id3v2Tag )
{
    m_fieldMap.insert( Meta::valAlbumArtist, TagLib::String( "TPE2" ) );
    m_fieldMap.insert( Meta::valBpm,         TagLib::String( "TBPM" ) );
    m_fieldMap.insert( Meta::valCompilation, TagLib::String( "TCMP" ) );
    m_fieldMap.insert( Meta::valComposer,    TagLib::String( "TCOM" ) );
    m_fieldMap.insert( Meta::valDiscNr,      TagLib::String( "TPOS" ) );
    m_fieldMap.insert( Meta::valHasCover,    TagLib::String( "APIC" ) );
    m_fieldMap.insert( Meta::valUniqueId,    TagLib::String( "UFID" ) );

    m_fmpsFieldMap.insert( FMPSPlayCount,    TagLib::String( "FMPS_Playcount" ) );
    m_fmpsFieldMap.insert( FMPSRating,       TagLib::String( "FMPS_Rating" ) );
    m_fmpsFieldMap.insert( FMPSScore,        TagLib::String( "FMPS_Rating_Amarok_Score" ) );

    m_uidFieldMap.insert( UIDAFT,            TagLib::String( "Amarok 2 AFTv1 - amarok.kde.org" ) );
    m_uidFieldMap.insert( UIDMusicBrainz,    TagLib::String( "http://musicbrainz.org" ) );
}

Meta::FieldHash
ID3v2TagHelper::tags() const
{
    Meta::FieldHash data = TagHelper::tags();

    TagLib::ID3v2::FrameList list = m_tag->frameList();
    for( TagLib::ID3v2::FrameList::ConstIterator it = list.begin(); it != list.end(); ++it )
    {
        qint64 field;
        TagLib::String frameName  = TagLib::String( ( *it )->frameID() );
        if( ( field = fieldName( frameName ) ) )
        {
            if( field == Meta::valUniqueId )
            {
                TagLib::ID3v2::UniqueFileIdentifierFrame *frame =
                        dynamic_cast< TagLib::ID3v2::UniqueFileIdentifierFrame * >( *it );

                if( !frame )
                    continue;

                QString identifier = TStringToQString( TagLib::String( frame->identifier() ) );

                if( identifier.isEmpty() )
                    continue;

                if( frame->owner() == uidFieldName( UIDAFT ) && isValidUID( identifier, UIDAFT ) )
                    data.insert( Meta::valUniqueId, identifier );
                else if( frame->owner() == uidFieldName( UIDMusicBrainz ) && isValidUID( identifier, UIDMusicBrainz ) )
                {
                    if( !data.contains( Meta::valUniqueId ) ) // we prefere AFT uids
                        data.insert( Meta::valUniqueId, identifier.prepend( "mb-" ) );
                }
                continue;
            }
            else if( field == Meta::valHasCover )
            {
                TagLib::ID3v2::AttachedPictureFrame *frame =
                        dynamic_cast< TagLib::ID3v2::AttachedPictureFrame * >( *it );

                if( !frame )
                    continue;

                if( ( frame->type() == TagLib::ID3v2::AttachedPictureFrame::FrontCover ||
                      frame->type() == TagLib::ID3v2::AttachedPictureFrame::Other ) &&
                    frame->picture().size() > 1024 ) // must be at least 1kb
                {
                    data.insert( Meta::valHasCover, true );
                }
                continue;
            }

            TagLib::ID3v2::TextIdentificationFrame *frame =
                    dynamic_cast< TagLib::ID3v2::TextIdentificationFrame * >( *it );

            if( !frame )
                continue;

            QString value = TStringToQString( frame->fieldList().toString( '\n' ) );

            if( field == Meta::valDiscNr )
            {
                int disc;
                if( ( disc = splitDiscNr( value ).first ) )
                    data.insert( field, disc );
            }
            else if( field == Meta::valBpm )
                data.insert( field, value.toFloat() );
            else
                data.insert( field, value );
        }
        else if( frameName == POPM_Frame )
        {
            TagLib::ID3v2::PopularimeterFrame *frame =
                    dynamic_cast< TagLib::ID3v2::PopularimeterFrame * >( *it );

            if( !frame )
                continue;

            if( TStringToQString( frame->email() ).isEmpty() ) // only read anonymous ratings
            {
                // FMPS tags have precedence
                if( !data.contains( Meta::valRating ) && frame->rating() != 0 )
                    data.insert( Meta::valRating, qRound( frame->rating() / 256.0 * 10.0 ) );
                if( !data.contains( Meta::valPlaycount ) && frame->counter() < 10000 )
                    data.insert( Meta::valPlaycount, frame->counter() );
            }
        }
        else if( frameName == TXXX_Frame )
        {
            TagLib::ID3v2::UserTextIdentificationFrame *frame =
                    dynamic_cast< TagLib::ID3v2::UserTextIdentificationFrame * >( *it );

            if( !frame )
                continue;

            // the value of the user text frame is stored in the
            // second and following fields.
            TagLib::StringList fields = frame->fieldList();
            if( fields.size() >= 2 )
            {
                QString value = TStringToQString( fields[1] );

                if( fields[0] == fmpsFieldName( FMPSRating ) )
                    data.insert( Meta::valRating, qRound( value.toFloat() * 10.0 ) );
                else if( fields[0] == fmpsFieldName( FMPSScore ) )
                    data.insert( Meta::valScore, value.toFloat() * 100.0 );
                else if( fields[0] == fmpsFieldName( FMPSPlayCount ) )
                    data.insert( Meta::valPlaycount, value.toFloat() );
            }
        }
    }

    return data;
}

bool
ID3v2TagHelper::setTags( const Meta::FieldHash &changes )
{
    bool modified = TagHelper::setTags( changes );

    foreach( const qint64 key, changes.keys() )
    {
        QVariant value = changes.value( key );
        TagLib::ByteVector field( fieldName( key ).toCString() );

        if( !field.isNull() && !field.isEmpty() )
        {
            if( key == Meta::valHasCover )
                continue;
            else if( key == Meta::valUniqueId )
            {
                QPair< UIDType, QString > uidPair = splitUID( value.toString() );
                if( uidPair.first == UIDInvalid )
                    continue;

                TagLib::String owner  = uidFieldName( uidPair.first );
                TagLib::ByteVector uid( uidPair.second.toAscii().data() );
                TagLib::ID3v2::FrameList list = m_tag->frameList();

                for( TagLib::ID3v2::FrameList::ConstIterator it = list.begin(); it != list.end(); ++it )
                {
                    if( ( *it )->frameID() == field )
                    {
                        TagLib::ID3v2::UniqueFileIdentifierFrame *frame =
                                dynamic_cast< TagLib::ID3v2::UniqueFileIdentifierFrame * >( *it );
                        if( !frame )
                            continue;

                        if( frame->owner() == owner )
                        {
                            m_tag->removeFrame( frame );
                            modified = true;
                            break;
                        }
                    }
                }

                if ( !uid.isEmpty() )
                {
                    m_tag->addFrame( new TagLib::ID3v2::UniqueFileIdentifierFrame( owner, uid ) );
                    modified = true;
                }
                continue;
            }

            TagLib::String tValue = Qt4QStringToTString( ( key == Meta::valCompilation )
                                                         ? QString::number( value.toInt() )
                                                         : value.toString() );
            if( tValue.isEmpty() )
                m_tag->removeFrames( field );
            else
            {
                TagLib::ID3v2::TextIdentificationFrame *frame = NULL;
                if( !m_tag->frameListMap()[field].isEmpty() )
                    frame = dynamic_cast< TagLib::ID3v2::TextIdentificationFrame * >(
                                                  m_tag->frameListMap()[field].front()
                                                                                    );
                if( !frame )
                {
                    frame = new TagLib::ID3v2::TextIdentificationFrame( field );
                    m_tag->addFrame( frame );
                }
                // note: TagLib is smart enough to automatically set UTF8 encoding if needed.
                frame->setText( tValue );
            }
            modified = true;
        }
        else if( key == Meta::valScore || key == Meta::valRating || key == Meta::valPlaycount )
        {
            TagLib::String description;
            TagLib::String tValue;

            if( key == Meta::valRating )
            {
                description = fmpsFieldName( FMPSRating );
                tValue = Qt4QStringToTString( QString::number( value.toFloat() / 10.0 ) );
            }
            else if( key == Meta::valScore )
            {
                description = fmpsFieldName( FMPSScore );
                tValue = Qt4QStringToTString( QString::number( value.toFloat() / 100.0 ) );
            }
            else if( key == Meta::valPlaycount )
            {
                description = fmpsFieldName( FMPSPlayCount );
                tValue = Qt4QStringToTString( QString::number( value.toInt() ) );
            }

            if( key == Meta::valRating || key == Meta::valPlaycount )
            {
                TagLib::ID3v2::PopularimeterFrame *popFrame = NULL;
                if( !m_tag->frameListMap()[POPM_Frame].isEmpty() )
                    popFrame = dynamic_cast< TagLib::ID3v2::PopularimeterFrame * >( m_tag->frameListMap()[POPM_Frame].front() );

                if( !popFrame )
                {
                    popFrame = new TagLib::ID3v2::PopularimeterFrame( POPM_Frame );
                    m_tag->addFrame( popFrame );
                }

                if( key == Meta::valRating )
                    popFrame->setRating( qBound(0, int(qRound(value.toDouble() / 10.0 * 256)), 255) );
                else
                    popFrame->setCounter( value.toInt() );

                modified = true;
            }

            TagLib::ID3v2::FrameList list = m_tag->frameList();
            for( TagLib::ID3v2::FrameList::ConstIterator it = list.begin(); it != list.end(); ++it )
            {
                if( ( *it )->frameID() == TXXX_Frame )
                {
                    TagLib::ID3v2::UserTextIdentificationFrame *frame =
                            dynamic_cast< TagLib::ID3v2::UserTextIdentificationFrame * >( *it );
                    if( !frame )
                        continue;

                    if( frame->description() == description )
                    {
                        m_tag->removeFrame( frame );
                        modified = true;
                        break;
                    }
                }
            }

            if( value.toBool() )
            {
                TagLib::ID3v2::UserTextIdentificationFrame *frame =
                        new TagLib::ID3v2::UserTextIdentificationFrame( TXXX_Frame );

                frame->setDescription( description );
                frame->setText( tValue );
                m_tag->addFrame( frame );
                modified = true;
            }
        }
    }

    return modified;
}

TagLib::ByteVector
ID3v2TagHelper::render() const
{
    return m_tag->render();
}

#ifndef UTILITIES_BUILD
bool
ID3v2TagHelper::hasEmbeddedCover() const
{
    TagLib::ByteVector field = fieldName( Meta::valHasCover ).toCString();
    if( m_tag->frameListMap().contains( field ) )
    {
        TagLib::ID3v2::AttachedPictureFrame *frame =
                dynamic_cast< TagLib::ID3v2::AttachedPictureFrame * >( m_tag->frameListMap()[field].front() );
        return frame && ( frame->picture().size() > 1024 ) &&
               ( frame->type() == TagLib::ID3v2::AttachedPictureFrame::FrontCover ||
                 frame->type() == TagLib::ID3v2::AttachedPictureFrame::Other );
    }
    return false;
}

QImage
ID3v2TagHelper::embeddedCover() const
{
    TagLib::ID3v2::FrameList apicList = m_tag->frameListMap()[fieldName( Meta::valHasCover ).toCString()];
    TagLib::ID3v2::AttachedPictureFrame *frontCover = NULL;
    TagLib::ID3v2::AttachedPictureFrame *otherCover = NULL;
    TagLib::ID3v2::AttachedPictureFrame *coverToUse = NULL;

    uint maxSize = 1024;     // ignore images that are too small

    for( TagLib::ID3v2::FrameList::ConstIterator iter = apicList.begin(); iter != apicList.end(); ++iter )
    {
        TagLib::ID3v2::AttachedPictureFrame *currFrame =
                dynamic_cast< TagLib::ID3v2::AttachedPictureFrame * >( *iter );

        if( currFrame->picture().size() < maxSize )
            continue;

        if( currFrame->type() == TagLib::ID3v2::AttachedPictureFrame::FrontCover )
        {
            frontCover = currFrame;
            maxSize = currFrame->picture().size();
        }
        else if( currFrame->type() == TagLib::ID3v2::AttachedPictureFrame::Other )
        {
            otherCover = currFrame;
            maxSize = currFrame->picture().size();
        }
    }

    if( !frontCover && !otherCover )
        return QImage();

    //If Front and Other covers have the same size, we should use the Front one.
    if( frontCover && !otherCover )
        coverToUse = frontCover;
    else if( !frontCover && otherCover )
        coverToUse = otherCover;
    else if( frontCover->picture().size() >= otherCover->picture().size() )
        coverToUse = frontCover;
    else
        coverToUse = otherCover;

    return QImage::fromData( ( uchar * )( coverToUse->picture().data() ),
                             coverToUse->picture().size() );
}

bool
ID3v2TagHelper::setEmbeddedCover( const QImage &cover )
{
    QByteArray bytes;
    QBuffer buffer( &bytes );

    buffer.open( QIODevice::WriteOnly );

    if( !cover.save( &buffer, "JPEG" ) )
    {
        buffer.close();
        return false;
    }

    buffer.close();

    TagLib::ByteVector field = fieldName( Meta::valHasCover ).toCString();
    TagLib::ID3v2::FrameList apicList = m_tag->frameListMap()[field];
    TagLib::ID3v2::AttachedPictureFrame *frontCover = NULL;

    for( TagLib::ID3v2::FrameList::ConstIterator iter = apicList.begin(); iter != apicList.end(); ++iter )
    {
        TagLib::ID3v2::AttachedPictureFrame *currFrame =
                dynamic_cast< TagLib::ID3v2::AttachedPictureFrame * >( *iter );

        if( currFrame->type() == TagLib::ID3v2::AttachedPictureFrame::FrontCover )
        {
            frontCover = currFrame;
            break;
        }
    }

    if( !frontCover )
    {
        frontCover = new TagLib::ID3v2::AttachedPictureFrame( field );
        frontCover->setType( TagLib::ID3v2::AttachedPictureFrame::FrontCover );
        m_tag->addFrame( frontCover );
    }

    frontCover->setMimeType( "image/jpeg" );
    frontCover->setPicture( TagLib::ByteVector( bytes.data(), bytes.count() ) );
    return true;
}
#endif  //UTILITIES_BUILD
