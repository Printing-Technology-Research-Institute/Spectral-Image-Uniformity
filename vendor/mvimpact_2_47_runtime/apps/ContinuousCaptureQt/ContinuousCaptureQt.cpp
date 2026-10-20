#include "ContinuousCaptureQt.h"
#include <common/STLHelper.h>

#include <apps/Common/qtIncludePrologue.h>
#include <QDebug>
#include <QStyleFactory>
#include <QMessageBox>
#include <apps/Common/qtIncludeEpilogue.h>

//-----------------------------------------------------------------------------
ContinuousCaptureQt::ContinuousCaptureQt( QWidget* parent ) : QMainWindow( parent ),
    pCaptureThread_( 0 ), pImageCanvas_( 0 ), devMgr_(), pThread_( 0 ), lock_(), pFI_( 0 ), pAC_( 0 )
//-----------------------------------------------------------------------------
{
    setupUi( this );
    icon_label->setPixmap( QPixmap( QString::fromUtf8( ":/bulb-off.png" ) ) );
    sliderExposureTime->setVisible( false );
    label_exposureSlider->setVisible( false );
    pImageCanvas_ = new ImageCanvas( label_image );
    populateCombobox();
}
//-----------------------------------------------------------------------------
ContinuousCaptureQt::~ContinuousCaptureQt()
//-----------------------------------------------------------------------------
{
    cleanUp();
    DeleteElement( pImageCanvas_ );
}

//-----------------------------------------------------------------------------
void ContinuousCaptureQt::cleanUp( Device* pDev /* = 0 */ )
//-----------------------------------------------------------------------------
{
    QMutexLocker lockedScope( &lock_ );
    if( pCaptureThread_ )
    {
        pCaptureThread_->terminate();
        if( pThread_ )
        {
            pThread_->terminate();
            pThread_->wait();
        }
    }
    if( pDev && pDev->isOpen() )
    {
        pDev->close();
    }
    pImageCanvas_->SetImage( 0 );
    DeleteElement( pCaptureThread_ );
    DeleteElement( pThread_ );
    DeleteElement( pFI_ );
    DeleteElement( pAC_ );
}

//-----------------------------------------------------------------------------
void ContinuousCaptureQt::closeEvent( QCloseEvent* pEvent )
//-----------------------------------------------------------------------------
{
    cleanUp( getDevice( comboBox->currentIndex() ) );
    pEvent->accept();
}

//-----------------------------------------------------------------------------
void ContinuousCaptureQt::closeSelectedDevice( void )
//-----------------------------------------------------------------------------
{
    try
    {
        cleanUp( getDevice( comboBox->currentIndex() ) );
        label_StatusMessage->setText( "" );
        close_button->setEnabled( false );
        comboBox->setEnabled( true );
        open_button->setEnabled( true );
        acquisition_button->setEnabled( false );
        icon_label->setPixmap( QPixmap( QString::fromUtf8( ":/bulb-off.png" ) ) );
    }
    catch( const ImpactAcquireException& e )
    {
        QString msg;
        QTextStream( &msg ) << "Error while closing device! Error message: " << QString::fromStdString( e.getErrorCodeAsString() ) << "\nError Code: " << e.getErrorCode();
        QMessageBox::warning( this, "Error", msg, QMessageBox::Ok, QMessageBox::Ok );
        text_label->setText( "ERROR CLOSING THE DEVICE" );
    }
}

//-----------------------------------------------------------------------------
void ContinuousCaptureQt::errorString( QString msg )
//-----------------------------------------------------------------------------
{
    label_StatusMessage->setText( msg );
    qDebug() << "Error! - " << msg;
}

//-----------------------------------------------------------------------------
void ContinuousCaptureQt::openSelectedDevice( void )
//-----------------------------------------------------------------------------
{
    try
    {
        Device* pDev = getDevice( comboBox->currentIndex() );
        conditionalSetProperty( pDev->interfaceLayout, dilGenICam );
        pDev->open();
        pFI_ = new FunctionInterface( pDev );
        if( pDev->interfaceLayout.isValid() && ( pDev->interfaceLayout.read() == dilGenICam ) )
        {
            pAC_ = new GenICam::AcquisitionControl( pDev );
        }
        setupExposureTimeSlider();
        comboBox->setEnabled( false );
        open_button->setEnabled( false );
        close_button->setEnabled( true );
        acquisition_button->setEnabled( true );
        icon_label->setPixmap( QPixmap( QString::fromUtf8( ":/bulb-on.png" ) ) );
    }
    catch( const ImpactAcquireException& e )
    {
        QString msg;
        QTextStream( &msg ) << "Error while opening device! Error message: " << QString::fromStdString( e.getErrorCodeAsString() ) << "\nError Code: " << e.getErrorCode();
        QMessageBox::warning( this, "Error", msg, QMessageBox::Ok, QMessageBox::Ok );
        text_label->setText( "ERROR OPENING THE DEVICE" );
    }
}

//-----------------------------------------------------------------------------
void ContinuousCaptureQt::populateCombobox( void )
//-----------------------------------------------------------------------------
{
    QStringList comboBoxEntries;
    for( unsigned int i = 0; i < devMgr_.deviceCount(); i++ )
    {
        Device* pDev = devMgr_.getDevice( i );
        QString entry;
        QTextStream( &entry ) << "[" << i << "] - " << QString::fromStdString( pDev->serial.readS() ) << " - " << QString::fromStdString( pDev->product.readS() );
        comboBoxEntries.append( entry );
    }
    comboBox->addItems( comboBoxEntries );
}

//-----------------------------------------------------------------------------
void ContinuousCaptureQt::requestReady( void )
//-----------------------------------------------------------------------------
{
    QMutexLocker lockedScope( &lock_ );
    if( pThread_ && pThread_->isRunning() )
    {
        const int requestNumber = pCaptureThread_->getPendingRequestNr();
        if( requestNumber != INVALID_ID )
        {
            pImageCanvas_->SetImage( pFI_->getRequest( requestNumber ) );
        }
    }
}

//-----------------------------------------------------------------------------
void ContinuousCaptureQt::setExposureTime( int value )
//-----------------------------------------------------------------------------
{
    if( pAC_ )
    {
        pAC_->exposureTime.write( value );
        qDebug() << "Set Exposure Time Value to " << value << " - New Value: " << pAC_->exposureTime.read();
    }
}

//-----------------------------------------------------------------------------
void ContinuousCaptureQt::setupExposureTimeSlider( void )
//-----------------------------------------------------------------------------
{
    if( pAC_ )
    {
        conditionalSetEnumPropertyByString( pAC_->exposureAuto, "Off" );
        if( pAC_->exposureTime.isValid() )
        {
            if( pAC_->exposureTime.hasMaxValue() )
            {
                sliderExposureTime->setMaximum( 100000 );
            }
            if( pAC_->exposureTime.hasMinValue() )
            {
                sliderExposureTime->setMinimum( static_cast< int >( pAC_->exposureTime.getMinValue() ) );
            }
        }
        sliderExposureTime->setSingleStep( 100 );
        sliderExposureTime->setSliderPosition( static_cast< int >( pAC_->exposureTime.read() ) );
        sliderExposureTime->setVisible( true );
        label_exposureSlider->setVisible( true );
    }
    else
    {
        sliderExposureTime->setVisible( false );
        label_exposureSlider->setVisible( false );
    }
}

//-----------------------------------------------------------------------------
void ContinuousCaptureQt::startAcquisition( void )
//-----------------------------------------------------------------------------
{
    pCaptureThread_ = new CaptureThread( getDevice( comboBox->currentIndex() ), false, pFI_ );

    pThread_ = new QThread;
    pCaptureThread_->moveToThread( pThread_ );

    // Signal emitted in case of acquisition errors
    connect( pCaptureThread_, SIGNAL( error( QString ) ), this, SLOT( errorString( QString ) ) );

    connect( pCaptureThread_, SIGNAL( finished() ), pThread_, SLOT( quit() ) );
    connect( pCaptureThread_, SIGNAL( finished() ), pCaptureThread_, SLOT( deleteLater() ) );

    // signal emitted in case of an acquired image ready to be displayed
    connect( pCaptureThread_, SIGNAL( requestReady() ), this, SLOT( requestReady() ) );

    // start signal for the capture thread
    connect( pThread_, SIGNAL( started() ), pCaptureThread_, SLOT( process() ) );

    // update signal for the statistics label
    connect( pCaptureThread_, SIGNAL( updateStatistics( const QString& ) ), this, SLOT( updateStatistics( const QString& ) ) );

    // update signal for the exposure time slider
    connect( sliderExposureTime, SIGNAL( valueChanged( int ) ), this, SLOT( setExposureTime( int ) ) );

    pThread_->start();
    if( pThread_->isRunning() )
    {
        acquisition_button->setEnabled( false );
    }
}

//-----------------------------------------------------------------------------
void ContinuousCaptureQt::userSelectedDeviceInComboBox( void )
//-----------------------------------------------------------------------------
{
    open_button->setEnabled( true );
    text_label->setText( "Please Select A Device:" );
}
