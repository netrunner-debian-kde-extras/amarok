/****************************************************************************************
 * Copyright (c) 2010 Ralf Engels <ralf-engels@gmx.de>                                  *
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

#include "core/meta/support/MetaConstants.h"
#include "shared/FileType.h"
#include <klocalizedstring.h>

QString Meta::nameForField( qint64 field )
{
    switch( field )
    {
    case 0:                    return "anything";
    case Meta::valUrl:         return "filename";
    case Meta::valTitle:       return "title";
    case Meta::valArtist:      return "artist";
    case Meta::valAlbum:       return "album";
    case Meta::valGenre:       return "genre";
    case Meta::valComposer:    return "composer";
    case Meta::valYear:        return "year";
    case Meta::valComment:     return "comment";
    case Meta::valTrackNr:     return "tracknr";
    case Meta::valDiscNr:      return "discnumber";
    case Meta::valBpm:         return "bpm";
    case Meta::valLength:      return "length";
    case Meta::valBitrate:     return "bitrate";
    case Meta::valSamplerate:  return "samplerate";
    case Meta::valFilesize:    return "filesize";
    case Meta::valFormat:      return "format";
    case Meta::valCreateDate:  return "added";
    case Meta::valScore:       return "score";
    case Meta::valRating:      return "rating";
    case Meta::valFirstPlayed: return "firstplay";
    case Meta::valLastPlayed:  return "lastplay";
    case Meta::valPlaycount:   return "playcount";
    case Meta::valUniqueId:    return "uniqueid";

    case Meta::valTrackGain:   return "trackgain";
    case Meta::valTrackGainPeak:   return "trackgainpeak";
    case Meta::valAlbumGain:   return "albumgain";
    case Meta::valAlbumGainPeak:   return "albumgainpeak";

    case Meta::valAlbumArtist: return "albumartist";
    case Meta::valLabel:       return "label";
    case Meta::valModified:    return "modifydate";
    default:                   return QString();
    }
}

qint64 Meta::fieldForName( const QString &name )
{
    if( name == "anything" ) return 0;
    else if( name == "filename" ) return Meta::valUrl;
    else if( name == "title" ) return Meta::valTitle;
    else if( name == "artist" ) return Meta::valArtist;
    else if( name == "album" ) return Meta::valAlbum;
    else if( name == "genre" ) return Meta::valGenre;
    else if( name == "composer" ) return Meta::valComposer;
    else if( name == "year" ) return Meta::valYear;
    else if( name == "comment" ) return Meta::valComment;
    else if( name == "tracknumber" ) return Meta::valTrackNr;
    else if( name == "discnumber" ) return Meta::valDiscNr;
    else if( name == "bpm" ) return Meta::valBpm;
    else if( name == "length" ) return Meta::valLength;
    else if( name == "bitrate" ) return Meta::valBitrate;
    else if( name == "samplerate" ) return Meta::valSamplerate;
    else if( name == "filesize" ) return Meta::valFilesize;
    else if( name == "format" ) return Meta::valFormat;
    else if( name == "added" ) return Meta::valCreateDate;
    else if( name == "score" ) return Meta::valScore;
    else if( name == "rating" ) return Meta::valRating;
    else if( name == "firstplay" ) return Meta::valFirstPlayed;
    else if( name == "lastplay" ) return Meta::valLastPlayed;
    else if( name == "playcount" ) return Meta::valPlaycount;
    else if( name == "uniqueid" ) return Meta::valUniqueId;

    else if( name == "trackgain" ) return Meta::valTrackGain;
    else if( name == "trackgainpeak" ) return Meta::valTrackGainPeak;
    else if( name == "albumgain" ) return Meta::valAlbumGain;
    else if( name == "albumgainpeak" ) return Meta::valAlbumGainPeak;

    else if( name == "albumartist" ) return Meta::valAlbumArtist;
    else if( name == "label" ) return Meta::valLabel;
    else if( name == "modifydate" ) return Meta::valModified;
    else return 0;
}

QString Meta::i18nForField( qint64 field )
{
    switch( field )
    {
    case 0:                    return i18nc("The field name in case nothing specific is selected e.g. in the automatic playlist generator", "anything");
    case Meta::valUrl:         return i18nc( "The name of the file this track is stored in", "File Name" );
    case Meta::valTitle:       return i18n("Title");
    case Meta::valArtist:      return i18n("Artist Name");
    case Meta::valAlbum:       return i18n("Album Name");
    case Meta::valGenre:       return i18n("Genre");
    case Meta::valComposer:    return i18n("Composer");
    case Meta::valYear:        return i18n("Year");
    case Meta::valComment:     return i18n("Comment");
    case Meta::valTrackNr:     return i18n("Track Number");
    case Meta::valDiscNr:      return i18n("Disc Number");
    case Meta::valBpm:         return i18n("Bpm");
    case Meta::valLength:      return i18n("Length");
    case Meta::valBitrate:     return i18n("Bit Rate");
    case Meta::valSamplerate:  return i18n("Sample Rate");
    case Meta::valFilesize:    return i18n("File Size");
    case Meta::valFormat:      return i18n("Format");
    case Meta::valCreateDate:  return i18n("Added to Collection");
    case Meta::valScore:       return i18n("Score");
    case Meta::valRating:      return i18n("Rating");
    case Meta::valFirstPlayed: return i18n("First Played");
    case Meta::valLastPlayed:  return i18n("Last Played");
    case Meta::valPlaycount:   return i18n("Playcount");
    case Meta::valUniqueId:    return i18n("Unique Id");

    case Meta::valTrackGain:   return i18n("Track Gain");
    case Meta::valTrackGainPeak:   return i18n("Track Gain Peak");
    case Meta::valAlbumGain:   return i18n("Album Gain");
    case Meta::valAlbumGainPeak:   return i18n("Album Gain Peak");

    case Meta::valAlbumArtist: return i18n("Album Artist Name");
    case Meta::valLabel:       return i18n("Label");
    case Meta::valModified:    return i18n("Last Modified");
    default:                   return QString();
    }
}

QString Meta::shortI18nForField( qint64 field )
{
    // see also src/browsers/CollectionTreeItemModelBase.cpp for localized names

    switch( field )
    {
    case 0:                    return i18nc("The field name in case nothing specific is selected e.g. in the automatic playlist generator. Use a one word translation.", "anything");
    case Meta::valUrl:         return i18nc("One word translation used in the collection filter. The name of the file this track is stored in", "filename" );
    case Meta::valTitle:       return i18nc("One word translation used in the collection filter", "title");
    case Meta::valArtist:      return i18nc("One word translation used in the collection filter", "artist");
    case Meta::valAlbum:       return i18nc("One word translation used in the collection filter", "album");
    case Meta::valGenre:       return i18nc("One word translation used in the collection filter", "genre");
    case Meta::valComposer:    return i18nc("One word translation used in the collection filter", "composer");
    case Meta::valYear:        return i18nc("One word translation used in the collection filter", "year");
    case Meta::valComment:     return i18nc("One word translation used in the collection filter", "comment");
    case Meta::valTrackNr:     return i18nc("One word translation used in the collection filter", "tracknumber");
    case Meta::valDiscNr:      return i18nc("One word translation used in the collection filter", "discnumber");
    case Meta::valBpm:         return i18nc("One word translation used in the collection filter", "bpm");
    case Meta::valLength:      return i18nc("One word translation used in the collection filter", "length");
    case Meta::valBitrate:     return i18nc("One word translation used in the collection filter", "bitrate");
    case Meta::valSamplerate:  return i18nc("One word translation used in the collection filter", "samplerate");
    case Meta::valFilesize:    return i18nc("One word translation used in the collection filter", "filesize");
    case Meta::valFormat:      return i18nc("One word translation used in the collection filter", "format");
    case Meta::valCreateDate:  return i18nc("One word translation used in the collection filter", "added");
    case Meta::valScore:       return i18nc("One word translation used in the collection filter", "score");
    case Meta::valRating:      return i18nc("One word translation used in the collection filter", "rating");
    case Meta::valFirstPlayed: return i18nc("One word translation used in the collection filter. First played time / access date", "firstplay");
    case Meta::valLastPlayed:  return i18nc("One word translation used in the collection filter. Last played time / access date", "lastplay");
    case Meta::valPlaycount:   return i18nc("One word translation used in the collection filter", "playcount");
    case Meta::valUniqueId:    return i18nc("One word translation used in the collection filter", "uniqueid");

    case Meta::valTrackGain:   return i18nc("One word translation used in the collection filter", "trackgain");
    case Meta::valTrackGainPeak:   return i18nc("One word translation used in the collection filter", "trackgainpeak");
    case Meta::valAlbumGain:   return i18nc("One word translation used in the collection filter", "albumgain");
    case Meta::valAlbumGainPeak:   return i18nc("One word translation used in the collection filter", "albumgainpeak");

    case Meta::valAlbumArtist: return i18nc("One word translation used in the collection filter", "albumartist");
    case Meta::valLabel:       return i18nc("One word translation used in the collection filter", "label");
    case Meta::valModified:    return i18nc("One word translation used in the collection filter", "modified");
    default:                   return QString();
    }
}

QString Meta::playlistNameForField( qint64 field )
{
    switch( field )
    {
    case 0:                    return "anything";
    case Meta::valUrl:         return "url";
    case Meta::valTitle:       return "title";
    case Meta::valArtist:      return "artist name";
    case Meta::valAlbum:       return "album name";
    case Meta::valGenre:       return "genre";
    case Meta::valComposer:    return "composer";
    case Meta::valYear:        return "year";
    case Meta::valComment:     return "comment";
    case Meta::valTrackNr:     return "track number";
    case Meta::valDiscNr:      return "disc number";
    case Meta::valBpm:         return "bpm";
    case Meta::valLength:      return "length";
    case Meta::valBitrate:     return "bit rate";
    case Meta::valSamplerate:  return "sample rate";
    case Meta::valFilesize:    return "file size";
    case Meta::valFormat:      return "format";
    case Meta::valCreateDate:  return "create date";
    case Meta::valScore:       return "score";
    case Meta::valRating:      return "rating";
    case Meta::valFirstPlayed: return "first played";
    case Meta::valLastPlayed:  return "last played";
    case Meta::valPlaycount:   return "play count";
    case Meta::valUniqueId:    return "unique id";

    case Meta::valTrackGain:   return "track gain";
    case Meta::valTrackGainPeak:   return "track gain peak";
    case Meta::valAlbumGain:   return "album gain";
    case Meta::valAlbumGainPeak:   return "album gain peak";

    case Meta::valAlbumArtist: return "album artist name";
    case Meta::valLabel:       return "label";
    case Meta::valModified:    return "modifydate";
    default:                   return "";
    }
}

qint64 Meta::fieldForPlaylistName( const QString &name )
{
    if( name == "anything" ) return 0;
    else if( name == "url" ) return Meta::valUrl;
    else if( name == "title" ) return Meta::valTitle;
    else if( name == "artist name" ) return Meta::valArtist;
    else if( name == "album name" ) return Meta::valAlbum;
    else if( name == "genre" ) return Meta::valGenre;
    else if( name == "composer" ) return Meta::valComposer;
    else if( name == "year" ) return Meta::valYear;
    else if( name == "comment" ) return Meta::valComment;
    else if( name == "track number" ) return Meta::valTrackNr;
    else if( name == "disc number" ) return Meta::valDiscNr;
    else if( name == "bpm" ) return Meta::valBpm;
    else if( name == "length" ) return Meta::valLength;
    else if( name == "bit rate" ) return Meta::valBitrate;
    else if( name == "sample rate" ) return Meta::valSamplerate;
    else if( name == "file size" ) return Meta::valFilesize;
    else if( name == "format" ) return Meta::valFormat;
    else if( name == "create date" ) return Meta::valCreateDate;
    else if( name == "score" ) return Meta::valScore;
    else if( name == "rating" ) return Meta::valRating;
    else if( name == "first played" ) return Meta::valFirstPlayed;
    else if( name == "last played" ) return Meta::valLastPlayed;
    else if( name == "play count" ) return Meta::valPlaycount;
    else if( name == "unique id" ) return Meta::valUniqueId;

    else if( name == "track gain" ) return Meta::valTrackGain;
    else if( name == "track gain peak" ) return Meta::valTrackGainPeak;
    else if( name == "album gain" ) return Meta::valAlbumGain;
    else if( name == "album gain peak" ) return Meta::valAlbumGainPeak;

    else if( name == "album artist name" ) return Meta::valAlbumArtist;
    else if( name == "label" ) return Meta::valLabel;
    else if( name == "modifydate" ) return Meta::valModified;
    else return 0;
}

QString Meta::iconForField( qint64 field )
{
    switch( field )
    {
    case Meta::valUrl: return "filename-space-amarok";
    case Meta::valTitle: return "filename-title-amarok";
    case Meta::valArtist:
    case Meta::valAlbumArtist: return "filename-artist-amarok";
    case Meta::valAlbum: return "filename-album-amarok";
    case Meta::valGenre: return "filename-genre-amarok";
    case Meta::valComposer: return "filename-composer-amarok";
    case Meta::valYear:
    case Meta::valModified:
    case Meta::valCreateDate: return "filename-year-amarok";
    case Meta::valComment:
    case Meta::valPlaycount: return "filename-comment-amarok";
    case Meta::valTrackNr: return "filename-track-amarok";
    case Meta::valDiscNr: return "filename-discnumber-amarok";
    case Meta::valBpm: return "filename-bpm-amarok";
    case Meta::valLength: return "filename-group-length";
    case Meta::valBitrate: return "application-octet-stream";
    case Meta::valSamplerate: return "filename-sample-rate";
    case Meta::valFormat: return "filename-filetype-amarok";
    case Meta::valScore: return "emblem-favorite";
    case Meta::valRating: return "rating";
    case Meta::valFirstPlayed:
    case Meta::valLastPlayed: return "filename-last-played";
    case Meta::valLabel: return "label-amarok";
    case Meta::valFilesize: return "info-amarok";
    default: return QString();
    }
}

QVariant Meta::valueForField( qint64 field, Meta::TrackPtr track )
{
    if( !track )
        return QVariant();

    switch( field )
    {
    case 0:
    {
        // that is the simple search for MetaQueryWidget
        QSet<QString> allInfos;
        allInfos += track->playableUrl().path()
            += track->name()
            += track->comment();
        if( track->artist() )
            allInfos += track->artist()->name();
        if( track->album() )
            allInfos += track->album()->name();
        if( track->genre() )
            allInfos += track->genre()->name();

        return QVariant( allInfos.toList() );
    }
    case Meta::valUrl:         return track->playableUrl().path();
    case Meta::valTitle:       return track->name();
    case Meta::valArtist:      return track->artist() ?
                               QVariant(track->artist()->name()) : QVariant();
    case Meta::valAlbum:       return track->album() ?
                               QVariant(track->album()->name()) : QVariant();
    case Meta::valGenre:       return track->genre() ?
                               QVariant(track->genre()->name()) : QVariant();
    case Meta::valComposer:    return track->composer() ?
                               QVariant(track->composer()->name()) : QVariant();
    case Meta::valYear:        return track->year() ?
                               QVariant(track->year()->name().toInt()) : QVariant();
    case Meta::valComment:     return track->comment();
    case Meta::valTrackNr:     return track->trackNumber();
    case Meta::valDiscNr:      return track->discNumber();
    case Meta::valBpm:         return track->bpm();
    case Meta::valLength:      return track->length();
    case Meta::valBitrate:     return track->bitrate();
    case Meta::valSamplerate:  return track->sampleRate();
    case Meta::valFilesize:    return track->filesize();
    case Meta::valFormat:      return int(Amarok::FileTypeSupport::fileType(track->type()));

    case Meta::valCreateDate:  return track->createDate();
    case Meta::valScore:       return track->score();
    case Meta::valRating:      return track->rating();
    case Meta::valFirstPlayed: return track->firstPlayed();
    case Meta::valLastPlayed:  return track->lastPlayed();
    case Meta::valPlaycount:   return track->playCount();
    case Meta::valUniqueId:    return track->uidUrl();

    // todo
    case Meta::valTrackGain:   return "track gain";
    case Meta::valTrackGainPeak:   return "track gain peak";
    case Meta::valAlbumGain:   return "album gain";
    case Meta::valAlbumGainPeak:   return "album gain peak";

    case Meta::valAlbumArtist: return (track->album() && track->album()->albumArtist()) ?
                               QVariant(track->album()->albumArtist()->name()) : QVariant();
    case Meta::valLabel:
      {
          Meta::LabelList labels = track->labels();
          QStringList strLabels;
          foreach( Meta::LabelPtr label, labels )
              strLabels.append( label->name() );
          return QVariant( strLabels );
      }
    case Meta::valModified:  return track->modifyDate();
    default: return QVariant();
    }
}


