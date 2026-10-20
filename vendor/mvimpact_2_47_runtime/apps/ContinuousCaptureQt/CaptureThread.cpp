#include "CaptureThread.h"

//-----------------------------------------------------------------------------
CaptureThread::CaptureThread( Device* pCurrDev, bool boTerminated, FunctionInterface* pFI_ ) :
    pDev_( pCurrDev ), boTerminated_( boTerminated ), requestPendingForDisplay_( INVALID_ID ), pFI_( pFI_ )
//-----------------------------------------------------------------------------
{
}

//-----------------------------------------------------------------------------
void CaptureThread::process( void )
//-----------------------------------------------------------------------------
{
    try
    {
        TDMR_ERROR result = DMR_NO_ERROR;
        while( ( result = static_cast< TDMR_ERROR >( pFI_->imageRequestSingle() ) ) == DMR_NO_ERROR ) {};
        if( result != DEV_NO_FREE_REQUEST_AVAILABLE )
        {
            qDebug() << "'FunctionInterface.imageRequestSingle' returned with an unexpected result: " << QString::fromStdString( mvIMPACT::acquire::ImpactAcquireException::getErrorCodeAsString( result ) );
        }
        manuallyStartAcquisitionIfNeeded( pDev_, *pFI_ );
        int cnt = 0;
        // run thread loop
        mvIMPACT::acquire::Statistics statistics( pDev_ );
        mvIMPACT::acquire::Request* pRequest = 0;
        const unsigned int timeout_ms = 200;
        while( !boTerminated_ )
        {
            const int requestNr = pFI_->imageRequestWaitFor( timeout_ms );
            pRequest = pFI_->isRequestNrValid( requestNr ) ? pFI_->getRequest( requestNr ) : 0;
            if( pRequest )
            {
                {
                    QMutexLocker lockedScope( &lock_ );
                    if( pRequest->isOK() )
                    {
                        if( requestPendingForDisplay_ != INVALID_ID )
                        {
                            // another request is already pending for being displayed by the GUI so simply drop this one
                            pRequest->unlock();
                        }
                        else
                        {
                            // use the current request as the next one for displaying...
                            requestPendingForDisplay_ = pRequest->getNumber();
                            // ... and tell the GUI to pick it up
                            emit requestReady();
                        }
                        cnt++;
                        // display some statistics
                        if( cnt % 10 == 0 )
                        {
                            QString data;
                            QTextStream( &data ) << "Frame Count: " << cnt << "\n"
                                                 << QString::fromStdString( statistics.framesPerSecond.name() ) << ": " << QString::fromStdString( statistics.framesPerSecond.readS() ) << "\n"
                                                 << QString::fromStdString( pRequest->infoExposeTime_us.name() ) << ": " << QString::fromStdString( pRequest->infoExposeTime_us.readS() );
                            emit updateStatistics( data );
                        }
                    }
                    else
                    {
                        pRequest->unlock();
                    }
                }
                while( ( result = static_cast< TDMR_ERROR >( pFI_->imageRequestSingle() ) ) == DMR_NO_ERROR ) {};
                if( result != DEV_NO_FREE_REQUEST_AVAILABLE )
                {
                    qDebug() << "'FunctionInterface.imageRequestSingle' returned with an unexpected result: " << QString::fromStdString( mvIMPACT::acquire::ImpactAcquireException::getErrorCodeAsString( result ) );
                }
            }
            //else
            //{
            // Please note that slow systems or interface technologies in combination with high resolution sensors
            // might need more time to transmit an image than the timeout value which has been passed to imageRequestWaitFor().
            // If this is the case simply wait multiple times OR increase the timeout(not recommended as usually not necessary
            // and potentially makes the capture thread less responsive) and rebuild this application.
            // Once the device is configured for triggered image acquisition and the timeout elapsed before
            // the device has been triggered this might happen as well.
            // The return code would be -2119(DEV_WAIT_FOR_REQUEST_FAILED) in that case, the documentation will provide
            // additional information under TDMR_ERROR in the interface reference.
            // If waiting with an infinite timeout(-1) it will be necessary to call 'imageRequestReset' from another thread
            // to force 'imageRequestWaitFor' to return when no data is coming from the device/can be captured.
            // qDebug() << "imageRequestWaitFor failed (" << requestNr << ", " << QString::fromStdString( ImpactAcquireException::getErrorCodeAsString( requestNr ) ) << ")" << ", timeout value too small?";
            //  emit error( QString::fromStdString( ImpactAcquireException::getErrorCodeAsString( requestNr ) ) );
            //}
        }
        manuallyStopAcquisitionIfNeeded( pDev_, *pFI_ );
        if( pRequest )
        {
            pRequest->unlock();
        }
        pFI_->imageRequestReset( 0, 0 );
    }
    catch( const ImpactAcquireException& e )
    {
        emit error( QString::fromStdString( e.getErrorCodeAsString() ) );
    }
}

//-----------------------------------------------------------------------------
int CaptureThread::getPendingRequestNr( void )
//-----------------------------------------------------------------------------
{
    QMutexLocker lockedScope( &lock_ );
    int result = requestPendingForDisplay_;
    // Reset the ID of the request to tell the capture loop that this request has already been
    // picked up and someone else will take care of it from now on.
    requestPendingForDisplay_ = INVALID_ID;
    return result;
}
