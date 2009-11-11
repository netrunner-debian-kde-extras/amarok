/***************************************************************************
 *   Copyright (c) 2009 Sven Krohlas <sven@getamarok.com>                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.         *
 ***************************************************************************/

#ifndef TESTMETAMULTITRACK_H
#define TESTMETAMULTITRACK_H

#include <QtCore/QStringList>

namespace Meta {
    class MultiTrack;
}

class TestMetaMultiTrack : public QObject
{
Q_OBJECT

public:
    TestMetaMultiTrack( QStringList testArgumentList, bool stdout );

private slots:
    void initTestCase();
    void cleanupTestCase();

    void testFirst();
    void testNext();
    void testCurrentAndSetSource();
    void testSources();
    void testHasCapabilityInterface();

private:
    Meta::MultiTrack *m_testMultiTrack;
};

#endif // TESTMETAMULTITRACK_H
