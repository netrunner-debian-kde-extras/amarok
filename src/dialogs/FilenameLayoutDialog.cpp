/****************************************************************************************
 * Copyright (c) 2008 Téo Mrnjavac <teo@kde.org>                                        *
 * Copyright (c) 2009 Nikolaj Hald Nielsen <nhn@kde.org>                                *
 * Copyright (c) 2009 Daniel Dewald <Daniel.Dewald@time.shift.de>                       *
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

#include "FilenameLayoutDialog.h"
#include "TokenDropTarget.h"

#include "core/support/Amarok.h"
#include "core/support/Debug.h"
#include "shared/MetaValues.h"

#include <KConfig>
#include <KColorScheme>


FilenameLayoutDialog::FilenameLayoutDialog( QWidget *parent, bool isOrganizeCollection )
    : QWidget( parent )
    , m_isOrganizeCollection( isOrganizeCollection )
    , m_advancedMode( false )
{
    setupUi( this );
    optionsFrame->hide();
    resultGroupBox->hide();

    m_caseEditRadioButtons << rbAllUpper << rbAllLower << rbFirstLetter << rbTitleCase;

    m_dropTarget = new TokenDropTarget( "application/x-amarok-tag-token", filenameLayout );
    m_dropTarget->setRowLimit( 1 );
    m_dropTarget->layout()->setContentsMargins( 1, 1, 1, 1 );
    QVBoxLayout *l = new QVBoxLayout(filenameLayout);
    l->setContentsMargins( 0, 0, 0, 0 );
    l->addWidget(m_dropTarget);

    filenameLayoutEdit->hide();
    syntaxLabel->hide();
    syntaxLabel->setWordWrap( true );

    connect( cbCase, SIGNAL( toggled( bool ) ),
             this, SLOT( editStateEnable( bool ) ) );
    connect( cbCase, SIGNAL( toggled( bool ) ),
             this, SLOT( updatePreview() ) );
    connect( rbTitleCase, SIGNAL( toggled(bool) ),
             this, SLOT( updatePreview() ) );
    connect( rbFirstLetter, SIGNAL( toggled(bool) ),
             this, SLOT( updatePreview() ) );
    connect( rbAllLower, SIGNAL( toggled(bool) ),
             this, SLOT( updatePreview() ) );
    connect( rbAllUpper, SIGNAL( toggled(bool) ),
             this, SLOT( updatePreview() ) );
    connect( cbEliminateSpaces, SIGNAL( toggled(bool) ),
             this, SLOT( updatePreview() ) );
    connect( cbReplaceUnderscores, SIGNAL( toggled(bool) ),
             this, SLOT( updatePreview() ) );
    connect( tokenPool, SIGNAL( onDoubleClick( Token * ) ),
             m_dropTarget, SLOT( insertToken( Token* ) ) );
    connect( kpbAdvanced, SIGNAL( clicked() ),
             this, SLOT( toggleAdvancedMode() ) );
    connect( m_dropTarget, SIGNAL( changed() ),
             this, SIGNAL( schemeChanged() ) );
    connect( m_dropTarget, SIGNAL( changed() ),
             this, SLOT( updatePreview() ) );
    connect( filenameLayoutEdit, SIGNAL( textChanged( const QString & ) ),
             this, SIGNAL( schemeChanged() ) );
    connect( filenameLayoutEdit, SIGNAL( textChanged( const QString & ) ),
             this, SLOT( updatePreview() ) );
    connect( m_dropTarget, SIGNAL( changed() ),
             this, SLOT( updatePreview() ) );
    connect( cbUseFullPath, SIGNAL( toggled( bool ) ),
             this, SLOT( updatePreview() ) );
    connect( sbNestingLevel, SIGNAL( valueChanged( int ) ),
             this, SLOT( updatePreview() ) );


    //KConfig stuff:
    int caseOptions = Amarok::config( "TagGuesser" ).readEntry( "Case options" ).toInt();
    if( !caseOptions )
        cbCase->setChecked( false );
    else
    {
        cbCase->setChecked( true );
        if( caseOptions == 1 )
            rbAllLower->setChecked( true );
        else if( caseOptions == 2 )
            rbAllUpper->setChecked( true );
        else if( caseOptions == 3 )
            rbFirstLetter->setChecked( true );
        else if( caseOptions == 4 )
            rbTitleCase->setChecked( true );
        else
            debug() << "OUCH";
    }

    int fullFilePathOptions = Amarok::config( "TagGuesser" ).readEntry( "Use full file path" ).toInt();
    cbUseFullPath->setChecked( fullFilePathOptions );
    int whitespaceOptions = Amarok::config( "TagGuesser" ).readEntry( "Eliminate trailing spaces" ).toInt();
    cbEliminateSpaces->setChecked( whitespaceOptions );
    int underscoreOptions = Amarok::config( "TagGuesser" ).readEntry( "Replace underscores" ).toInt();
    cbReplaceUnderscores->setChecked( underscoreOptions );
    if( !m_isOrganizeCollection )
    {
        optionsFrame->show();
        resultGroupBox->show();
    }
    else
    {
        //INIT for collection root
        filenamePreview->hide();
        unsigned int borderColor = static_cast<unsigned int>( KColorScheme( QPalette::Active ).decoration( KColorScheme::HoverColor ).color().rgb() );
        collectionRootFrame->setStyleSheet( "\
            color: palette( Text );\
            border: 2px solid #" + QString::number( borderColor, 16 ).remove( 0, 2 ) + ";\
            border-radius: 4px;\
            padding: 2px;\
            " );
        QHBoxLayout *collectionRootLayout = new QHBoxLayout( collectionRootFrame );
        QLabel *collectionRootIconLabel = new QLabel( "", this );
        QLabel *collectionRootLabel = new QLabel( i18n( "Collection root" ), this );
        collectionRootLayout->addWidget( collectionRootIconLabel );
        collectionRootLayout->addWidget( collectionRootLabel );
        collectionRootLabel->setStyleSheet( "border:0px solid #000000; border-radius: 0px; padding: 0px;" );
        collectionRootIconLabel->setStyleSheet( "border:0px solid #000000; border-radius: 0px; padding: 0px;" );
        collectionRootLayout->setContentsMargins( 0, 0, 0, 0 );
        collectionRootIconLabel->setContentsMargins( 0, 0, 0, 0 );
        collectionRootLabel->setContentsMargins( 0, 0, 0, 0 );
        collectionRootIconLabel->setSizePolicy( QSizePolicy::Fixed, QSizePolicy::Fixed );
        collectionRootIconLabel->setFixedSize( 16, 16 );
        QPixmap collectionIcon = QPixmap( KIcon( "collection-amarok" ).pixmap(16, 16) );
        collectionRootIconLabel->setPixmap( collectionIcon );

        collectionSlashFrame->setStyleSheet( "\
            color: palette( Text );\
            border: 2px solid #" + QString::number( borderColor, 16 ).remove( 0, 2 ) + ";\
            border-radius: 4px;\
            padding: 2px;\
            " );
        QHBoxLayout *collectionSlashLayout = new QHBoxLayout( collectionSlashFrame );
        QLabel *collectionSlashLabel = new QLabel( "/", this );
        collectionSlashLayout->addWidget(collectionSlashLabel);
        collectionSlashLabel->setStyleSheet( "border:0px solid #000000; border-radius: 0px; padding: 0px;" );
        collectionSlashLayout->setContentsMargins( 0, 0, 0, 0 );
        collectionSlashLabel->setContentsMargins( 0, 0, 0, 0 );

        dotFrame->setStyleSheet( "\
            color: palette( Text );\
            border: 2px solid #" + QString::number( borderColor, 16 ).remove( 0, 2 ) + ";\
            border-radius: 4px;\
            padding: 2px;\
            " );

        QHBoxLayout *dotLayout = new QHBoxLayout( dotFrame );
        QLabel *dotIconLabel = new QLabel( "", this );
        QLabel *dotLabel = new QLabel( "." , this );
        dotLayout->addWidget( dotIconLabel );
        dotLayout->addWidget( dotLabel );
        dotLabel->setStyleSheet( "border:0px solid #000000; border-radius: 0px; padding: 0px;" );
        dotIconLabel->setStyleSheet( "border:0px solid #000000; border-radius: 0px; padding: 0px;" );
        dotLayout->setContentsMargins( 0, 0, 0, 0 );
        dotIconLabel->setContentsMargins( 0, 0, 0, 0 );
        dotLabel->setContentsMargins( 0, 0, 0, 0 );
        dotIconLabel->setSizePolicy( QSizePolicy::Fixed, QSizePolicy::Fixed );
        dotIconLabel->setFixedSize( 16, 16 );
        QPixmap dotIcon = QPixmap( KIcon( "filename-dot-amarok" ).pixmap(16, 16) );
        dotIconLabel->setPixmap( dotIcon );


        extensionFrame->setStyleSheet( "\
            color: palette( Text );\
            border: 2px solid #" + QString::number( borderColor, 16 ).remove( 0, 2 ) + ";\
            border-radius: 4px;\
            padding: 2px;\
            " );

        QHBoxLayout *extensionLayout = new QHBoxLayout( extensionFrame );
        QLabel *extensionIconLabel = new QLabel( "", this );
        QLabel *extensionLabel = new QLabel( i18n("File Type"), this );
        extensionLayout->addWidget( extensionIconLabel );
        extensionLayout->addWidget( extensionLabel );
        extensionLabel->setStyleSheet( "border:0px solid #000000; border-radius: 0px; padding: 0px;" );
        extensionIconLabel->setStyleSheet( "border:0px solid #000000; border-radius: 0px; padding: 0px;" );
        extensionLayout->setContentsMargins( 0, 0, 0, 0 );
        extensionIconLabel->setContentsMargins( 0, 0, 0, 0 );
        extensionLabel->setContentsMargins( 0, 0, 0, 0 );
        extensionIconLabel->setSizePolicy( QSizePolicy::Fixed, QSizePolicy::Fixed );
        extensionIconLabel->setFixedSize( 16, 16 );
        QPixmap extensionIcon = QPixmap( KIcon( "filename-filetype-amarok" ).pixmap(16, 16) );
        extensionIconLabel->setPixmap( extensionIcon );
    }

    if( !m_isOrganizeCollection )
    {
        m_color_Album = QColor( album_color );
        m_color_Artist = QColor( artist_color );
        m_color_AlbumArtist = QColor( albumartist_color );
        m_color_Comment = QColor( comment_color );
        m_color_Composer = QColor( composer_color );
        m_color_Genre = QColor( genre_color );
        m_color_Title = QColor( title_color );
        m_color_Track = QColor( track_color );
        m_color_Year = QColor( year_color );
    }
    else
    {
        m_color_Album = m_color_Artist = m_color_AlbumArtist = m_color_Comment = \
        m_color_Composer = m_color_Genre = m_color_Title = m_color_Track = m_color_Year = Qt::black;
    }

    //INIT for tokenPool
    Token *nToken = new Token( i18n( "Track" ),"filename-track-amarok", Track );
    nToken->setTextColor( m_color_Track );
    tokenPool->addToken( nToken );

    nToken = new Token( i18n( "Title" ), "filename-title-amarok", Title );
    nToken->setTextColor( m_color_Title );
    tokenPool->addToken( nToken );

    nToken = new Token( i18n( "Artist" ), "filename-artist-amarok", Artist );
    nToken->setTextColor( m_color_Artist );
    tokenPool->addToken( nToken );

    nToken = new Token( i18n( "Composer" ), "filename-composer-amarok", Composer );
    nToken->setTextColor( m_color_Composer );
    tokenPool->addToken( nToken );

    nToken = new Token( i18n( "Year" ), "filename-year-amarok", Year );
    nToken->setTextColor( m_color_Year );
    tokenPool->addToken( nToken );

    nToken = new Token( i18n( "Album" ), "filename-album-amarok", Album );
    nToken->setTextColor( m_color_Album );
    tokenPool->addToken( nToken );

    nToken = new Token( i18n( "Album Artist" ), "filename-artist-amarok", AlbumArtist );
    nToken->setTextColor( m_color_AlbumArtist );
    tokenPool->addToken( nToken );

    nToken = new Token( i18n( "Comment" ), "filename-comment-amarok", Comment );
    nToken->setTextColor( m_color_Comment );
    tokenPool->addToken( nToken );

    nToken = new Token( i18n( "Genre" ), "filename-genre-amarok", Genre );
    nToken->setTextColor( m_color_Genre );
    tokenPool->addToken( nToken );

    tokenPool->addToken( new Token( "/", "filename-slash-amarok", Slash ) );

    tokenPool->addToken( new Token( "_", "filename-underscore-amarok", Underscore ) );
    tokenPool->addToken( new Token( "-", "filename-dash-amarok", Dash ) );

    Token *dotToken = new Token( ".", "filename-dot-amarok", Dot );
    tokenPool->addToken( dotToken );
    tokenPool->addToken( new Token( " ", "filename-space-amarok", Space ) );

    if( !m_isOrganizeCollection )
    {
        tokenPool->addToken( new Token( i18n( "Ignore" ), "filename-ignore-amarok", Ignore ) );
        syntaxLabel->setText( i18nc("Please do not translate the %foo% words as they define a syntax used internally by a parser to describe a filename.",
                                    // xgettext: no-c-format
                                    "The following tokens can be used to define a filename scheme:<br> \
                                     <font color=\"%1\">%track%</font>, <font color=\"%2\">%title%</font>, \
                                     <font color=\"%3\">%artist%</font>, <font color=\"%4\">%composer%</font>, \
                                     <font color=\"%5\">%year%</font>, <font color=\"%6\">%album%</font>, \
                                     <font color=\"%7\">%albumartist%</font>, <font color=\"%8\">%comment%</font>, \
                                     <font color=\"%9\">%genre%</font>, %ignore%."
                                     , m_color_Track.name(), m_color_Title.name(), m_color_Artist.name(), \
				     m_color_Composer.name(), m_color_Year.name(), m_color_Album.name(), m_color_AlbumArtist.name(), \
				     m_color_Comment.name(), m_color_Genre.name() ) );
    }
    else
    {
        tokenPool->addToken( new Token( i18nc( "Artist's Initial", "Initial" ), "filename-initial-amarok", Initial ) );
        Token *filetypeToken = new Token( i18n( "File type" ), "filename-filetype-amarok", FileType );
        tokenPool->addToken( filetypeToken );
        tokenPool->addToken( new Token( i18n( "Disc number" ), "filename-discnumber-amarok", DiscNumber ) );
        syntaxLabel->setText( i18nc("Please do not translate the %foo% words as they define a syntax used internally by a parser to describe a filename.",
                                    // xgettext: no-c-format
                                    "The following tokens can be used to define a filename scheme: \
                                     <br>%track%, %title%, %artist%, %composer%, %year%, %album%, %albumartist%, %comment%, %genre%, %initial%, %folder%, %filetype%, %discnumber%." ) );

    }
    if( m_isOrganizeCollection )
    {
        debug() << "about to infer!";
        if( Amarok::config( "OrganizeCollectionDialog" ).readEntry( "Mode" ) == "Advanced" )
        {
            debug() << "Advanced";
            setAdvancedMode( true );
            filenameLayoutEdit->setText( Amarok::config( "OrganizeCollectionDialog" ).readEntryUntranslated( "Custom Scheme" ) );
        }
        else// if( Amarok::config( "OrganizeCollectionDialog" ).readEntry( "Mode" ) == "Basic" )
        {
            debug() << "Basic";
            setAdvancedMode( false );
            QString scheme = Amarok::config( "OrganizeCollectionDialog" ).readEntryUntranslated( "Custom Scheme" );
            if(scheme.isEmpty())
                scheme = Amarok::config( "OrganizeCollectionDialog" ).readEntryUntranslated("Custom Scheme", "%artist%/%album%/%track%_-_%title%");
            inferScheme( scheme );
        }
    }
    else
    {
        if( Amarok::config( "FilenameLayoutDialog" ).readEntry( "Mode" ) == "Advanced" )
        {
            setAdvancedMode( true );
            filenameLayoutEdit->setText( Amarok::config( "FilenameLayoutDialog" ).readEntryUntranslated( "Custom Scheme" ) );
        }
        else if( Amarok::config( "FilenameLayoutDialog" ).readEntry( "Mode" ) == "Basic" )
        {
            setAdvancedMode( false );
            inferScheme( Amarok::config( "FilenameLayoutDialog" ).readEntryUntranslated( "Custom Scheme" ) );
        }
    }
}

//Stores the configuration when the dialog is accepted.
void
FilenameLayoutDialog::onAccept()    //SLOT
{
    Amarok::config( "TagGuesser" ).writeEntry( "Case options", getCaseOptions() );
    Amarok::config( "TagGuesser" ).writeEntry( "Eliminate trailing spaces", getWhitespaceOptions() );
    Amarok::config( "TagGuesser" ).writeEntry( "Replace underscores", getUnderscoreOptions() );
    Amarok::config( "TagGuesser" ).writeEntry( "Use full file path", cbUseFullPath->isChecked() );
}

//Forwards the request for a scheme to TokenLayoutWidget
QString
FilenameLayoutDialog::getParsableScheme()
{
    QString category = m_isOrganizeCollection ? "OrganizeCollectionDialog" : "FilenameLayoutDialog";
    QString scheme   = m_advancedMode ? filenameLayoutEdit->text() : parsableScheme();

    Amarok::config( category ).writeEntry( "Custom Scheme", scheme );
    return scheme;
}

//Sets Filename for Preview
void
FilenameLayoutDialog::setFileName( QString FileName )
{
    m_filename = FileName;
    sbNestingLevel->setMaximum( FileName.count( '/' ) - 1 );
    sbNestingLevel->setValue( 0 );
    updatePreview();
}

// attempts to set the scheme
void FilenameLayoutDialog::setScheme(const QString& scheme)
{
    if( m_advancedMode )
        filenameLayoutEdit->setText( scheme );
    else
        inferScheme( scheme );
}


//Updates the Filename Preview
void
FilenameLayoutDialog::updatePreview()                 //SLOT
{
    if( m_isOrganizeCollection )
       return;

    QString scheme = this->getParsableScheme();
    QFileInfo fi( m_filename );

    if( !scheme.isEmpty() )
    {
        TagGuesser guesser;
        guesser.setFilename( parsableFileName( fi ) );
        guesser.setCaseType( this->getCaseOptions() );
        guesser.setConvertUnderscores( this->getUnderscoreOptions() );
        guesser.setCutTrailingSpaces( this->getWhitespaceOptions() );
        guesser.setSchema( scheme );

        if( guesser.guess() )
        {
            QMap<qint64,QString> tags = guesser.tags();

            if( tags.contains( Meta::valAlbum ) )
                Album_result->setText( "<font color='" + QColor( album_color ).name() + "'>" + tags[Meta::valAlbum] + "</font>" );
            else
                Album_result->setText( i18nc( "Text to represent an empty tag. Braces (<>) are only to clarify emptiness.", "&lt;empty&gt;" ) );

            if( tags.contains( Meta::valAlbumArtist ) )
                AlbumArtist_result->setText( "<font color='" + QColor( albumartist_color ).name() + "'>" + tags[Meta::valAlbumArtist] + "</font>" );
            else
                AlbumArtist_result->setText( i18nc( "Text to represent an empty tag. Braces (<>) are only to clarify emptiness.", "&lt;empty&gt;" ) );

            if( tags.contains( Meta::valTitle ) )
                Title_result->setText( "<font color='" + QColor( title_color ).name() + "'>" + tags[Meta::valTitle] + "</font>" );
            else
                Title_result->setText( i18nc( "Text to represent an empty tag. Braces (<>) are only to clarify emptiness.", "&lt;empty&gt;" ) );

            if( tags.contains( Meta::valArtist ) )
                Artist_result->setText( "<font color='" + QColor( artist_color ).name() + "'>" + tags[Meta::valArtist] + "</font>" );
            else
                Artist_result->setText( i18nc( "Text to represent an empty tag. Braces (<>) are only to clarify emptiness.", "&lt;empty&gt;" ) );

            if( tags.contains( Meta::valComment ) )
                Comment_result->setText( "<font color='" + QColor( comment_color ).name() + "'>" + tags[Meta::valComment] + "</font>" );
            else
                Comment_result->setText( i18nc( "Text to represent an empty tag. Braces (<>) are only to clarify emptiness.", "&lt;empty&gt;" ) );

            if( tags.contains( Meta::valComposer ) )
                Composer_result->setText( "<font color='" + QColor( composer_color ).name() + "'>" + tags[Meta::valComposer] + "</font>" );
            else
                Composer_result->setText( i18nc( "Text to represent an empty tag. Braces (<>) are only to clarify emptiness.", "&lt;empty&gt;" ) );

            if( tags.contains( Meta::valGenre ) )
                Genre_result->setText( "<font color='" + QColor( genre_color ).name() + "'>" + tags[Meta::valGenre] + "</font>" );
            else
                Genre_result->setText( i18nc( "Text to represent an empty tag. Braces (<>) are only to clarify emptiness.", "&lt;empty&gt;" ) );

            if( tags.contains( Meta::valTrackNr ) )
                Track_result->setText( "<font color='" + QColor( track_color ).name() + "'>" + tags[Meta::valTrackNr] + "</font>" );
            else
                Track_result->setText( i18nc( "Text to represent an empty tag. Braces (<>) are only to clarify emptiness.", "&lt;empty&gt;" ) );

            if( tags.contains( Meta::valYear ) )
                Year_result->setText( "<font color='" + QColor( year_color ).name() + "'>" + tags[Meta::valYear] + "</font>" );
            else
                Year_result->setText( i18nc( "Text to represent an empty tag. Braces (<>) are only to clarify emptiness.", "&lt;empty&gt;" ) );

            filenamePreview->setText(guesser.coloredFileName());
        }
        else
        {
            filenamePreview->setText( parsableFileName( fi ) );
        }
    }
    else
    {
        filenamePreview->setText( parsableFileName( fi ) );
    }
}

//Handles the radiobuttons
void
FilenameLayoutDialog::editStateEnable( bool checked )      //SLOT
{
    foreach( QRadioButton *rb, m_caseEditRadioButtons )
        rb->setEnabled( checked );
}

//Returns a code for the configuration.
int
FilenameLayoutDialog::getCaseOptions()
{
    //Amarok::config( "TagGuesser" ).readEntry( "Filename schemes", QStringList() );
    if( !cbCase->isChecked() )
        return 0;
    else
    {
        if( rbAllLower->isChecked() )
            return 4;
        else if( rbAllUpper->isChecked() )
            return 3;
        else if( rbFirstLetter->isChecked() )
            return 2;
        else if( rbTitleCase->isChecked() )
            return 1;
        else
        {
            debug() << "OUCH!";
            return 0;
        }
    }
}

//As above
bool
FilenameLayoutDialog::getWhitespaceOptions()
{
    if( !cbEliminateSpaces->isChecked() )
        return false;
    return true;
}

//As above
bool
FilenameLayoutDialog::getUnderscoreOptions()
{
    if( !cbReplaceUnderscores->isChecked() )
        return false;
    return true;
}

//Handles the modifications to the dialog to toggle between advanced and basic editing mode.
void
FilenameLayoutDialog::toggleAdvancedMode()
{
    setAdvancedMode( !m_advancedMode );
}

//handles switching between basic and advanced mode
void
FilenameLayoutDialog::setAdvancedMode( bool isAdvanced )
{
    m_advancedMode = isAdvanced;

    if( isAdvanced )
    {
        kpbAdvanced->setText( i18n( "&Basic..." ) );
        filenameLayout->hide();
        filenameLayoutEdit->show();
        filenameLayoutEdit->setText( parsableScheme() );
        tokenPool->hide();
        syntaxLabel->show();

    }
    else // set Basic mode
    {
        kpbAdvanced->setText( i18n( "&Advanced..." ) );
        filenameLayout->show();
        filenameLayoutEdit->hide();
        tokenPool->show();
        syntaxLabel->hide();
        inferScheme( filenameLayoutEdit->text() );
    }

    QString configValue = m_isOrganizeCollection ? "OrganizeCollectionDialog" : "FilenameLayoutDialog";
    QString entryValue  = m_advancedMode ? "Advanced" : "Basic";

    Amarok::config( configValue ).writeEntry( "Mode", entryValue );
}



// Iterates over the elements of the TokenLayoutWidget bar
// (really over the elements of a QList that stores the indexes
// of the tokens) and generates a string that TagGuesser can digest.
QString
FilenameLayoutDialog::parsableScheme() const
{
    QString parsableScheme = "";

    QList< Token *> list = m_dropTarget->drags( 0 );

    foreach( Token *token, list )
    {
        parsableScheme += typeElements[token->value()];
    }

    return parsableScheme;
}



//tries to populate the widget with tokens according to a string
void
FilenameLayoutDialog::inferScheme( const QString &s ) //SLOT
{
    DEBUG_BLOCK

    debug() << "infering scheme: " << s;

    m_dropTarget->clear();
    for( int i = 0; i < s.size(); )
    {
        if( s.at(i) == '%')
        {
            if( s.mid( i, 7 ) == "%title%" )
            {
                Token *nToken = new Token( i18n( "Title" ), "filename-title-amarok", Title );
                nToken->setTextColor( m_color_Title );
                m_dropTarget->insertToken( nToken );
                i += 7;
            }
            else if( s.mid( i, 7 ) == "%track%" )
            {
                Token *nToken = new Token( i18n( "Track" ), "filename-track-amarok", Track );
                nToken->setTextColor( m_color_Track );
                m_dropTarget->insertToken( nToken );
                i += 7;
            }
            else if( s.mid( i, 8 ) == "%artist%" )
            {
                Token *nToken = new Token( i18n( "Artist" ), "filename-artist-amarok", Artist );
                nToken->setTextColor( m_color_Artist );
                m_dropTarget->insertToken( nToken );
                i += 8;
            }
            else if( s.mid( i, 10 ) == "%composer%" )
            {
                Token *nToken = new Token( i18n( "Composer" ), "filename-composer-amarok", Composer );
                nToken->setTextColor( m_color_Composer );
                m_dropTarget->insertToken( nToken );
                i += 10;
            }
            else if( s.mid( i, 6 ) == "%year%" )
            {
                Token *nToken = new Token( i18n( "Year" ), "filename-year-amarok", Year );
                nToken->setTextColor( m_color_Year );
                m_dropTarget->insertToken( nToken );
                i += 6;
            }
            else if( s.mid( i, 13 ) == "%albumartist%" )
            {
                Token *nToken = new Token( i18n( "Album Artist" ), "filename-artist-amarok", AlbumArtist );
                nToken->setTextColor( m_color_Album );
                m_dropTarget->insertToken( nToken );
                i += 13;
            }
            else if( s.mid( i, 7 ) == "%album%" )
            {
                Token *nToken = new Token( i18n( "Album" ), "filename-album-amarok", Album );
                nToken->setTextColor( m_color_Album );
                m_dropTarget->insertToken( nToken );
                i += 7;
            }
            else if( s.mid( i, 9 ) == "%comment%" )
            {
                Token *nToken = new Token( i18n( "Comment" ), "filename-comment-amarok", Comment );
                nToken->setTextColor( m_color_Comment );
                m_dropTarget->insertToken( nToken );
                i += 9;
            }
            else if( s.mid( i, 7 ) == "%genre%" )
            {
                Token *nToken = new Token( i18n( "Genre" ), "filename-genre-amarok", Genre );
                nToken->setTextColor( m_color_Genre );
                m_dropTarget->insertToken( nToken );
                i += 7;
            }
            else if( s.mid( i, 10 ) == "%filetype%" )
            {
                m_dropTarget->insertToken( new Token( i18n( "File type" ), "filename-filetype-amarok", FileType ) );
                i += 10;
            }
            else if( s.mid( i, 8 ) == "%ignore%" )
            {
                m_dropTarget->insertToken( new Token( i18n( "Ignore" ), "filename-ignore-amarok", Ignore ) );
                i += 8;
            }
            else if( s.mid( i, 8 ) == "%folder%" )
            {
                m_dropTarget->insertToken( new Token( i18n( "Folder" ), "filename-folder-amarok", Folder ) );
                i += 8;
            }
            else if( s.mid( i, 9 ) == "%initial%" )
            {
                m_dropTarget->insertToken( new Token( i18nc( "Artist's Initial", "Initial" ), "filename-initial-amarok", Initial ) );
                i += 9;
            }
            else if( s.mid( i, 12 ) == "%discnumber%" )
            {
                m_dropTarget->insertToken( new Token( i18n( "Disc number" ), "filename-discnumber-amarok", DiscNumber ) );
                i += 12;
            }
            else
                ++i; // skip junk
        }
        else
        {
            if( s.at(i) == '_' )
                m_dropTarget->insertToken( new Token( "_", "filename-underscore-amarok", Underscore ) );
            else if( s.at(i) == '-' )
                m_dropTarget->insertToken( new Token( "-", "filename-dash-amarok", Dash ) );
            else if( s.at(i) == '.' )
                m_dropTarget->insertToken( new Token( ".", "filename-dot-amarok", Dot ) );
            else if( s.at(i) == ' ' )
                m_dropTarget->insertToken( new Token( " ", "filename-space-amarok", Space ) );
            else if( s.at(i) == '/' )
                m_dropTarget->insertToken( new Token( "/", "filename-slash-amarok", Slash ) );
            else
                debug() << "'" << s.at(i) << "' can't be represented as TokenLayoutWidget Token";
            i++;
        }
    }
}

QString
FilenameLayoutDialog::parsableFileName( const QFileInfo &fileInfo ) const
{
    if( !cbUseFullPath->isChecked() || !sbNestingLevel->value() )
        return fileInfo.fileName();

    QString path = fileInfo.absoluteFilePath();
    int pos, n = sbNestingLevel->value() + 1;

    for( pos = 0; pos < path.length() && n; pos++ )
        if( path[pos] == '/' )
            n--;

    return path.mid( pos );
}

