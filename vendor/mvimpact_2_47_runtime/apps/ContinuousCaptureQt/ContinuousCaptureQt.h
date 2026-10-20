//-----------------------------------------------------------------------------
#ifndef ContinuousCaptureQtH
#define ContinuousCaptureQtH ContinuousCaptureQtH
//-----------------------------------------------------------------------------
#include <apps/Common/qtIncludePrologue.h>
#include <QtWidgets/QMainWindow>
#include "ui_ContinuousCaptureQt.h"
#include <QCloseEvent>
#include <QThread>
#include <apps/Common/qtIncludeEpilogue.h>

#include "CaptureThread.h"

//-----------------------------------------------------------------------------
class ContinuousCaptureQt : public QMainWindow, public Ui::ContinuousCaptureQt
//-----------------------------------------------------------------------------
{
    Q_OBJECT

public:
    explicit ContinuousCaptureQt( QWidget* parent = 0 );
    ~ContinuousCaptureQt( void );

private:
    CaptureThread* pCaptureThread_;
    ImageCanvas* pImageCanvas_;
    DeviceManager devMgr_;
    QThread* pThread_ ;
    QMutex lock_;
    FunctionInterface* pFI_;
    GenICam::AcquisitionControl* pAC_;

    void cleanUp( Device* pDev = 0 );
    void closeEvent( QCloseEvent* pEvent );
    Device* getDevice( int deviceIndex ) const
    {
        return devMgr_[deviceIndex];
    }
    void populateCombobox( void );

private slots:
    void closeSelectedDevice( void );
    void errorString( QString msg );
    void openSelectedDevice( void );
    void requestReady( void );
    void setExposureTime( int deviceIndex );
    void setupExposureTimeSlider( void );
    void startAcquisition( void );
    void updateStatistics( const QString& statisticalData )
    {
        label_StatusMessage->setText( statisticalData );
    }
    void userSelectedDeviceInComboBox( void );
};
#endif // ContinuousCaptureQtH
