/****************************************************************************************
 * Copyright (c) 2009 Téo Mrnjavac <teo@kde.org>                                        *
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

#include "TranscodingController.h"

#include "formats/TranscodingNullFormat.h"
#include "formats/TranscodingAacFormat.h"
#include "formats/TranscodingAlacFormat.h"
#include "formats/TranscodingFlacFormat.h"
#include "formats/TranscodingMp3Format.h"
#include "formats/TranscodingVorbisFormat.h"
#include "formats/TranscodingWmaFormat.h"

using namespace Transcoding;

Controller::Controller( QObject *parent )
    : QObject( parent )
{
    m_formats.insert( JUST_COPY, new NullFormat( JUST_COPY ) );
    m_formats.insert( INVALID, new NullFormat( INVALID ) );
    m_formats.insert( AAC, new AacFormat() );
    m_formats.insert( ALAC, new AlacFormat() );
    m_formats.insert( FLAC, new FlacFormat() );
    m_formats.insert( MP3, new Mp3Format() );
    m_formats.insert( VORBIS, new VorbisFormat() );
    m_formats.insert( WMA2, new WmaFormat() );

    KProcess *verifyAvailability = new KProcess( this );
    verifyAvailability->setOutputChannelMode( KProcess::MergedChannels );
    verifyAvailability->setProgram( "ffmpeg" );
    *verifyAvailability << QString( "-codecs" );
    connect( verifyAvailability, SIGNAL( finished( int, QProcess::ExitStatus ) ),
             this, SLOT( onAvailabilityVerified( int, QProcess::ExitStatus ) ) );
    verifyAvailability->start();
}

Controller::~Controller()
{
    qDeleteAll( m_formats );
}

void
Controller::onAvailabilityVerified( int exitCode, QProcess::ExitStatus exitStatus ) //SLOT
{
    Q_UNUSED( exitCode )
    Q_UNUSED( exitStatus )
    QString output = qobject_cast< KProcess * >( sender() )->readAllStandardOutput().data();
    if( output.simplified().isEmpty() )
        return;
    foreach( Format *format, m_formats )
    {
        if( format->verifyAvailability( output ) )
            m_availableEncoders.insert( format->encoder() );
    }
    sender()->deleteLater();
}

Format *
Controller::format( Encoder encoder ) const
{
    Q_ASSERT(m_formats.contains( encoder ));
    return m_formats.value( encoder );
}

#include "TranscodingController.moc"
