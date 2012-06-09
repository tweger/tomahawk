/* === This file is part of Tomahawk Player - <http://tomahawk-player.org> ===
 *
 *   Copyright 2010-2011, Christian Muehlhaeuser <muesli@tomahawk-player.org>
 *   Copyright 2011, Leo Franchi <lfranchi@kde.org>
 *   Copyright 2010-2011, Jeff Mitchell <jeff@tomahawk-player.org>
 *
 *   Tomahawk is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   Tomahawk is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with Tomahawk. If not, see <http://www.gnu.org/licenses/>.
 */

#include "TwitterAccount.h"
#include "TwitterConfigWidget.h"
#include "accounts/twitter/TomahawkOAuthTwitter.h"
#include "libtomahawk/infosystem/InfoSystem.h"

#include "sip/SipPlugin.h"

#include <QTweetLib/qtweetaccountverifycredentials.h>
#include <QTweetLib/qtweetuser.h>
#include <QTweetLib/qtweetstatus.h>
#include <QTweetLib/qtweetusershow.h>

#include <QtCore/QtPlugin>

namespace Tomahawk
{

namespace Accounts
{

Account*
TwitterAccountFactory::createAccount( const QString& accountId )
{
    return new TwitterAccount( accountId.isEmpty() ? Tomahawk::Accounts::generateId( factoryId() ) : accountId );
}


TwitterAccount::TwitterAccount( const QString &accountId )
    : Account( accountId )
    , m_isAuthenticated( false )
    , m_isAuthenticating( false )
{
    setAccountServiceName( "Twitter" );
    setTypes( AccountTypes( StatusPushType | SipType ) );

    connect( this, SIGNAL( credentialsLoaded( QVariantHash ) ), this, SLOT( onCredentialsLoaded( QVariantHash ) ) );

    qDebug() << "Got cached peers:" << configuration() << configuration()[ "cachedpeers" ];

    m_configWidget = QWeakPointer< TwitterConfigWidget >( new TwitterConfigWidget( this, 0 ) );
    connect( m_configWidget.data(), SIGNAL( twitterAuthed( bool ) ), SLOT( configDialogAuthedSignalSlot( bool ) ) );

    m_twitterAuth = QWeakPointer< TomahawkOAuthTwitter >( new TomahawkOAuthTwitter( TomahawkUtils::nam(), this ) );
}


TwitterAccount::~TwitterAccount()
{

}


void
TwitterAccount::configDialogAuthedSignalSlot( bool authed )
{
    tDebug() << Q_FUNC_INFO;
    m_isAuthenticated = authed;
    if ( !m_credentials[ "username" ].toString().isEmpty() )
        setAccountFriendlyName( QString( "@%1" ).arg( m_credentials[ "username" ].toString() ) );
    syncConfig();
    emit configurationChanged();
}


void
TwitterAccount::onCredentialsLoaded( const QVariantHash &credentials )
{
    // Credentials loaded
    bool reload = false;
    if ( !credentials[ "oauthtoken" ].toString().isEmpty() && !credentials[ "oauthtokensecret" ].toString().isEmpty() &&
         ( m_credentials[ "oauthtoken" ] != credentials[ "oauthtoken"] || m_credentials[ "oauthtokensecret" ] != credentials[ "oauthtokensecret" ] ) )
        reload = true;

    m_credentials = credentials;

    if ( reload && enabled() )
    {
        qDebug() << "Twitter account got async load of credentials, authenticating now!";
        authenticate();
    }
}


void
TwitterAccount::setCredentials( const QVariantHash &credentials )
{
    m_credentials = credentials;

    saveCredentials( credentials );
}


Account::ConnectionState
TwitterAccount::connectionState() const
{
    return m_twitterSipPlugin.data()->connectionState();
}

SipPlugin*
TwitterAccount::sipPlugin()
{
    if ( m_twitterSipPlugin.isNull() )
    {
        qDebug() << "CHECKING:" << configuration() << configuration()[ "cachedpeers" ];
        m_twitterSipPlugin = QWeakPointer< TwitterSipPlugin >( new TwitterSipPlugin( this ) );

        connect( m_twitterSipPlugin.data(), SIGNAL( stateChanged( Tomahawk::Accounts::Account::ConnectionState ) ), this, SIGNAL( connectionStateChanged( Tomahawk::Accounts::Account::ConnectionState ) ) );
        return m_twitterSipPlugin.data();
    }
    return m_twitterSipPlugin.data();
}


Tomahawk::InfoSystem::InfoPluginPtr
TwitterAccount::infoPlugin()
{
    if ( m_twitterInfoPlugin.isNull() )
        m_twitterInfoPlugin = QWeakPointer< Tomahawk::InfoSystem::TwitterInfoPlugin >( new Tomahawk::InfoSystem::TwitterInfoPlugin( this ) );

    return Tomahawk::InfoSystem::InfoPluginPtr( m_twitterInfoPlugin.data() );
}


void
TwitterAccount::authenticate()
{
    // Since we need to have a chance for deletion (via the infosystem) to work on the info plugin, we put this on the event loop
    tDebug() << Q_FUNC_INFO;
    QTimer::singleShot( 0, this, SLOT( authenticateSlot() ) );
}


void
TwitterAccount::authenticateSlot()
{
    tDebug() << Q_FUNC_INFO;
    if ( m_twitterInfoPlugin.isNull() )
    {
        if ( infoPlugin() && Tomahawk::InfoSystem::InfoSystem::instance()->workerThread() )
        {
            infoPlugin().data()->moveToThread( Tomahawk::InfoSystem::InfoSystem::instance()->workerThread().data() );
            Tomahawk::InfoSystem::InfoSystem::instance()->addInfoPlugin( infoPlugin() );
        }
    }
    
    if ( m_isAuthenticating )
    {
        tDebug( LOGVERBOSE ) << Q_FUNC_INFO << "Already authenticating";
        return;
    }
    
    tDebug() << Q_FUNC_INFO << "credentials: " << m_credentials.keys();

    if ( m_credentials[ "oauthtoken" ].toString().isEmpty() || m_credentials[ "oauthtokensecret" ].toString().isEmpty() )
    {
        tDebug() << Q_FUNC_INFO << "TwitterSipPlugin has empty Twitter credentials; not connecting";
        return;
    }

    if ( refreshTwitterAuth() )
    {
        m_isAuthenticating = true;
        tDebug() << Q_FUNC_INFO << "Verifying credentials";
        QTweetAccountVerifyCredentials *credVerifier = new QTweetAccountVerifyCredentials( m_twitterAuth.data(), this );
        connect( credVerifier, SIGNAL( parsedUser( const QTweetUser & ) ), SLOT( connectAuthVerifyReply( const QTweetUser & ) ) );
        credVerifier->verify();
    }
}


void
TwitterAccount::deauthenticate()
{
    tDebug() << Q_FUNC_INFO;
    
    if ( m_twitterSipPlugin )
        sipPlugin()->disconnectPlugin();

    if ( m_twitterInfoPlugin )
        Tomahawk::InfoSystem::InfoSystem::instance()->removeInfoPlugin( m_twitterInfoPlugin.data() );

    m_isAuthenticated = false;
    m_isAuthenticating = false;
    
    emit nowDeauthenticated();
}



bool
TwitterAccount::refreshTwitterAuth()
{
    qDebug() << Q_FUNC_INFO << " begin";
    if( !m_twitterAuth.isNull() )
        delete m_twitterAuth.data();

    Q_ASSERT( TomahawkUtils::nam() != 0 );
    tDebug() << Q_FUNC_INFO << " with nam " << TomahawkUtils::nam();
    m_twitterAuth = QWeakPointer< TomahawkOAuthTwitter >( new TomahawkOAuthTwitter( TomahawkUtils::nam(), this ) );

    if( m_twitterAuth.isNull() )
      return false;

    m_twitterAuth.data()->setOAuthToken( m_credentials[ "oauthtoken" ].toString().toLatin1() );
    m_twitterAuth.data()->setOAuthTokenSecret( m_credentials[ "oauthtokensecret" ].toString().toLatin1() );

    return true;
}


void
TwitterAccount::connectAuthVerifyReply( const QTweetUser &user )
{
    m_isAuthenticating = false;
    if ( user.id() == 0 )
    {
        qDebug() << "TwitterAccount could not authenticate to Twitter";
        deauthenticate();
    }
    else
    {
        tDebug() << "TwitterAccount successfully authenticated to Twitter as user " << user.screenName();
        QVariantHash config = configuration();
        config[ "screenname" ] = user.screenName();
        setConfiguration( config );
        sync();

        sipPlugin()->connectPlugin();

        m_isAuthenticated = true;
        emit nowAuthenticated( m_twitterAuth, user );
    }
}


QPixmap
TwitterAccount::icon() const {
    return QPixmap( ":/twitter-icon.png" );
}


}

}

Q_EXPORT_PLUGIN2( Tomahawk::Accounts::AccountFactory, Tomahawk::Accounts::TwitterAccountFactory )
