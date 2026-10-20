//-----------------------------------------------------------------------------
#ifndef CaptureThreadH
#define CaptureThreadH CaptureThreadH
//-----------------------------------------------------------------------------
#include "mvIMPACT_CPP/mvIMPACT_acquire.h"
#include "mvIMPACT_CPP/mvIMPACT_acquire_GenICam.h"
#include "apps/Common/exampleHelper.h"
#include "ImageCanvas.h"

#include <apps/Common/qtIncludePrologue.h>
#include <QThread>
#include <qdebug.h>
#include <apps/Common/qtIncludeEpilogue.h>

//-----------------------------------------------------------------------------
class CaptureThread : public QObject
//-----------------------------------------------------------------------------
{
    Q_OBJECT

public:
    explicit CaptureThread( Device* pCurrDev, bool boTerminated, FunctionInterface* pFi );
    void terminate( void )
    {
        boTerminated_ = true;
    }
    int getPendingRequestNr();

signals:
    void error( QString err );
    void finished( void );
    void requestReady( void );
    void updateStatistics( const QString& data );

private slots:
    void process( void );

private:
    Device* pDev_;
    volatile bool boTerminated_;
    int requestPendingForDisplay_;
    FunctionInterface* pFI_;
    QMutex lock_;
};
#endif // CaptureThreadH
