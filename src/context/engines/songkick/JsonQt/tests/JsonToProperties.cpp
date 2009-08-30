/****************************************************************************************
 * Copyright (c) 2008 Frederick Emmott <mail@fredemmott.co.uk>                          *
 *                                                                                      *
 * This program is free software; you can redistribute it and/or modify it under        *
 * the terms of the GNU General Public License as published by the Free Software        *
 * Foundation; either version 2 of the License, or (at your option) any later           *
 * version.                                                                             *
 *                                                                                      *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY      *
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A      *
 * PARTICULAR PURPOSE. See the GNU General Pulic License for more details.              *
 *                                                                                      *
 * You should have received a copy of the GNU General Public License along with         *
 * this program.  If not, see <http://www.gnu.org/licenses/>.                           *
 ****************************************************************************************/

#include "JsonToProperties.h"

#include <QtTest/QtTest>

// This test assumes that JsonToVariant works correctly.
class JsonToProperties : public QObject
{
	Q_OBJECT;
	private slots:
		void initTestCase()
		{
			JsonQt::JsonToProperties::parse(
				"{"
					"\"objectName\": \"My JSON Properties Test Object\","
					"\"END\" : [\"OF\", \"TEST\"]"
				"}",
				this
			);
		}
		void testObjectName()
		{
			QCOMPARE(
				this->objectName(),
				QString("My JSON Properties Test Object")
			);
		}
};

QTEST_MAIN(JsonToProperties);

#include "moc_JsonToProperties.cxx"
