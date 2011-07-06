/****************************************************************************************
 * Copyright (c) 2007 Alexandre Pereira de Oliveira <aleprj@gmail.com>                  *
 * Copyright (c) 2007-2009 Maximilian Kossick <maximilian.kossick@googlemail.com>       *
 * Copyright (c) 2007 Nikolaj Hald Nielsen <nhn@kde.org>                                *
 * Copyright (c) 2011 Ralf Engels <ralf-engels@gmx.de>                                  *
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

#define DEBUG_PREFIX "TextualQueryFilter"

#include "TextualQueryFilter.h"
#include "Expression.h"

#include "shared/FileType.h"
#include "core/support/Debug.h"

#include <KLocale>

using namespace Meta;


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
Collections::addTextualFilter( Collections::QueryMaker *qm, const QString &filter )
{
    int validFilters = qm->validFilterMask();

    ParsedExpression parsed = ExpressionParser::parse( filter );
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
                    addDateFilter( Meta::valLastPlayed, compare, elem.negate, elem.text, qm );
                }
                else if( lcField.compare( "first", Qt::CaseInsensitive ) == 0 || lcField.compare( shortI18nForField( Meta::valFirstPlayed ), Qt::CaseInsensitive ) == 0 )
                {
                    addDateFilter( Meta::valFirstPlayed, compare, elem.negate, elem.text, qm );
                }
                else if( lcField.compare( "added", Qt::CaseInsensitive ) == 0 || lcField.compare( shortI18nForField( Meta::valCreateDate ), Qt::CaseInsensitive ) == 0 )
                {
                    addDateFilter( Meta::valCreateDate, compare, elem.negate, elem.text, qm );
                }
                else if( lcField.compare( "modified", Qt::CaseInsensitive ) == 0 || lcField.compare( shortI18nForField( Meta::valModified ), Qt::CaseInsensitive ) == 0 )
                {
                    addDateFilter( Meta::valModified, compare, elem.negate, elem.text, qm );
                }
            }
            qm->endAndOr();
        }
        qm->endAndOr();
    }
}

void
Collections::addDateFilter( qint64 field, Collections::QueryMaker::NumberComparison compare,
                            bool negate, const QString &text, Collections::QueryMaker *qm )
{
    const uint date = semanticDateTimeParser( text ).toTime_t();
    if( date == 0 )
        return;

    if( compare == Collections::QueryMaker::Equals )
    {
        // equal means, on the same day
        uint day = 24 * 60 * 60;
        if( negate )
        {
            qm->excludeNumberFilter( field, date - day, Collections::QueryMaker::GreaterThan );
            qm->excludeNumberFilter( field, date + day, Collections::QueryMaker::LessThan );
        }
        else
        {
            qm->addNumberFilter( field, date - day, Collections::QueryMaker::GreaterThan );
            qm->addNumberFilter( field, date + day, Collections::QueryMaker::LessThan );
        }
    }
    // note: for historic reasons the conditions for dates are inverted.
    else if( compare == Collections::QueryMaker::LessThan )
    {
        if( negate )
            qm->excludeNumberFilter( field, date, Collections::QueryMaker::GreaterThan );
        else
            qm->addNumberFilter( field, date, Collections::QueryMaker::GreaterThan );
    }
    else if( compare == Collections::QueryMaker::GreaterThan )
    {
        if( negate )
            qm->excludeNumberFilter( field, date, Collections::QueryMaker::LessThan );
        else
            qm->addNumberFilter( field, date, Collections::QueryMaker::LessThan );
    }
}


QDateTime
Collections::semanticDateTimeParser( const QString &text )
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

