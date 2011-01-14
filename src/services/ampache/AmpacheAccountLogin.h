/****************************************************************************************
 * Copyright (c) 2007 Nikolaj Hald Nielsen <nhn@kde.org>                                *
 *           (c) 2010 Ian Monroe <ian@monroe.nu>                                        *
 *                                                                                      *
 * This program is free software; you can redistribute it and/or modify it under        *
 * the terms of the GNU General Public License as published by the Free Software        *
 * Foundation; either version 2 of the License, or (at your option) version 3 or        *
 * any later version accepted by the membership of KDE e.V. (or its successor approved  *
 * by the membership of KDE e.V.), which shall act as a proxy defined in Section 14 of  *
 * version 3 of the license.                                                            *
 *                                                                                      *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY      *
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A      *
 * PARTICULAR PURPOSE. See the GNU General Public License for more details.             *
 *                                                                                      *
 * You should have received a copy of the GNU General Public License along with         *
 * this program.  If not, see <http://www.gnu.org/licenses/>.                           *
 ****************************************************************************************/


#ifndef AMPACHEACCOUNTLOGIN_H
#define AMPACHEACCOUNTLOGIN_H

#include "NetworkAccessManagerProxy.h"

#include <KUrl>
#include <kdemacros.h>

#include <QObject>

#define AMPACHE_ACCOUNT_EXPORT KDE_EXPORT

class AMPACHE_ACCOUNT_EXPORT AmpacheAccountLogin : public QObject
{
    Q_OBJECT
    public:
        AmpacheAccountLogin ( const QString& url, const QString& username, const QString& password, QWidget* parent = 0 );
        ~AmpacheAccountLogin();
        QString server() const { return m_server; } 
        QString sessionId() const { return m_sessionId; }
        bool authenticated() const { return m_authenticated; }
        void reauthenticate();
    signals:
        void loginSuccessful(); //!authentication was successful
        void finished(); //!authentication was or was not successful
    private slots:
        void authenticate( const KUrl &url, QByteArray data, NetworkAccessManagerProxy::Error e );
        void authenticationComplete( const KUrl &url, QByteArray data, NetworkAccessManagerProxy::Error e );
        void versionVerify( QByteArray data );
    
    private:
        KUrl m_xmlDownloadUrl;
        KUrl m_xmlVersionUrl;
        
        bool m_authenticated;
        QString m_server;
        QString m_username;
        QString m_password;
        QString m_sessionId;
        int m_version;
};

#endif // AMPACHEACCOUNTLOGIN_H
