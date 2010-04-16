/****************************************************************************************
 * Copyright (c) 2009 Seb Ruiz <ruiz@kde.org>                                           *
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

#ifndef MEDIADEVICE_DECORATOR_CAPABILITY_H
#define MEDIADEVICE_DECORATOR_CAPABILITY_H

#include "core/capabilities/Capability.h"
#include "core/capabilities/DecoratorCapability.h"

namespace Collections {
    class MediaDeviceCollection;
}

class QAction;

namespace Capabilities
{
    class MediaDeviceDecoratorCapability : public DecoratorCapability
    {
        Q_OBJECT

        public:
            MediaDeviceDecoratorCapability( Collections::MediaDeviceCollection *coll );

            virtual QList<QAction*> decoratorActions();

        private:
            Collections::MediaDeviceCollection *m_coll;
    };
}
#endif
