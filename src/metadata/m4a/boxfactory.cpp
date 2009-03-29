/***************************************************************************
    copyright            : (C) 2002, 2003, 2006 by Jochen Issing
    email                : jochen.issing@isign-softart.de
 ***************************************************************************/

/***************************************************************************
 *   This library is free software; you can redistribute it and/or modify  *
 *   it  under the terms of the GNU Lesser General Public License version  *
 *   2.1 as published by the Free Software Foundation.                     *
 *                                                                         *
 *   This library is distributed in the hope that it will be useful, but   *
 *   WITHOUT ANY WARRANTY; without even the implied warranty of            *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU     *
 *   Lesser General Public License for more details.                       *
 *                                                                         *
 *   You should have received a copy of the GNU Lesser General Public      *
 *   License along with this library; if not, write to the Free Software   *
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,            *
 *   MA  02110-1301  USA                                                   *
 ***************************************************************************/

#include "boxfactory.h"

#include "itunesalbbox.h"
#include "itunesartbox.h"
#include "itunescmtbox.h"
#include "itunescvrbox.h"
#include "itunesdatabox.h"
#include "itunesdaybox.h"
#include "itunesdiskbox.h"
#include "itunesgenbox.h"
#include "itunesgrpbox.h"
#include "itunesnambox.h"
#include "itunestmpobox.h"
#include "itunestrknbox.h"
#include "ituneswrtbox.h"
#include "mp4hdlrbox.h"
#include "mp4ilstbox.h"
#include "mp4mdiabox.h"
#include "mp4metabox.h"
#include "mp4minfbox.h"
#include "mp4moovbox.h"
#include "mp4mvhdbox.h"
#include "mp4skipbox.h"
#include "mp4stblbox.h"
#include "mp4stsdbox.h"
#include "mp4trakbox.h"
#include "mp4udtabox.h"
#include "tstring.h"

#include <iostream>

using namespace TagLib;

MP4::BoxFactory::BoxFactory()
{
}

MP4::BoxFactory::~BoxFactory()
{
}

//! factory function
MP4::Mp4IsoBox* MP4::BoxFactory::createInstance( TagLib::File* anyfile, MP4::Fourcc fourcc, TagLib::uint size, long offset ) const
{
  MP4::File * file = dynamic_cast<MP4::File *>(anyfile);
  if(!file)
    return 0;

  //std::cout << "creating box for: " << fourcc.toString() << std::endl;

  switch( fourcc )
  {
  case 0x6d6f6f76: // 'moov'
    return new MP4::Mp4MoovBox( file, fourcc, size, offset );
    break;
  case 0x6d766864: // 'mvhd'
    return new MP4::Mp4MvhdBox( file, fourcc, size, offset );
    break;
  case 0x7472616b: // 'trak'
    return new MP4::Mp4TrakBox( file, fourcc, size, offset );
    break;
  case 0x6d646961: // 'mdia'
    return new MP4::Mp4MdiaBox( file, fourcc, size, offset );
    break;
  case 0x6d696e66: // 'minf'
    return new MP4::Mp4MinfBox( file, fourcc, size, offset );
    break;
  case 0x7374626c: // 'stbl'
    return new MP4::Mp4StblBox( file, fourcc, size, offset );
    break;
  case 0x73747364: // 'stsd'
    return new MP4::Mp4StsdBox( file, fourcc, size, offset );
    break;
  case 0x68646c72: // 'hdlr'
    return new MP4::Mp4HdlrBox( file, fourcc, size, offset );
    break;
  case 0x75647461: // 'udta'
    return new MP4::Mp4UdtaBox( file, fourcc, size, offset );
    break;
  case 0x6d657461: // 'meta'
    return new MP4::Mp4MetaBox( file, fourcc, size, offset );
    break;
  case 0x696c7374: // 'ilst'
    return new MP4::Mp4IlstBox( file, fourcc, size, offset );
    break;
  case 0xa96e616d: // '_nam'
    return new MP4::ITunesNamBox( file, fourcc, size, offset );
    break;
  case 0xa9415254: // '_ART'
    return new MP4::ITunesArtBox( file, fourcc, size, offset );
    break;
  case 0xa9616c62: // '_alb'
    return new MP4::ITunesAlbBox( file, fourcc, size, offset );
    break;
  case 0xa967656e: // '_gen'
    return new MP4::ITunesGenBox( file, fourcc, size, offset );
    break;
  case 0x676e7265: // 'gnre'
    return new MP4::ITunesGenBox( file, fourcc, size, offset );
    break;
  case 0xa9646179: // '_day'
    return new MP4::ITunesDayBox( file, fourcc, size, offset );
    break;
  case 0x74726b6e: // 'trkn'
    return new MP4::ITunesTrknBox( file, fourcc, size, offset );
    break;
  case 0xa9636d74: // '_cmt'
    return new MP4::ITunesCmtBox( file, fourcc, size, offset );
    break;
  case 0xa9677270: // '_grp'
    return new MP4::ITunesGrpBox( file, fourcc, size, offset );
    break;
  case 0xa9777274: // '_wrt'
    return new MP4::ITunesWrtBox( file, fourcc, size, offset );
    break;
  case 0x6469736b: // 'disk'
    return new MP4::ITunesDiskBox( file, fourcc, size, offset );
    break;
  case 0x746d706f: // 'tmpo'
    return new MP4::ITunesTmpoBox( file, fourcc, size, offset );
    break;
  case 0x636f7672: // 'covr'
    return new MP4::ITunesCvrBox( file, fourcc, size, offset );
    break;
  case 0x64617461: // 'data'
    return new MP4::ITunesDataBox( file, fourcc, size, offset );
    break;
  default:
    return new MP4::Mp4SkipBox( file, fourcc, size, offset );
  }
}
