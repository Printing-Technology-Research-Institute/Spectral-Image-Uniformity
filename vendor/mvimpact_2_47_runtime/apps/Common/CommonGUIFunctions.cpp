#include "CommonGUIFunctions.h"

#include "wxIncludePrologue.h"
#include <wx/progdlg.h>
#include "wxIncludeEpilogue.h"

//-----------------------------------------------------------------------------
void RunThreadAndShowProgress( wxThread* pThread, wxProgressDialog* pDlg, unsigned long threadInterval_ms, int maxExecutionTime_ms )
//-----------------------------------------------------------------------------
{
    pThread->Create();
    pThread->Run();
    while( pThread->IsRunning() )
    {
        wxMilliSleep( threadInterval_ms );
        if( pDlg )
        {
            pDlg->Pulse();
        }
    }
    pThread->Wait();
    if( pDlg )
    {
        pDlg->Update( maxExecutionTime_ms );
    }
}

//-----------------------------------------------------------------------------
void UpdateDeviceListWithProgressMessage( wxWindow* pParent, const mvIMPACT::acquire::DeviceManager& devMgr )
//-----------------------------------------------------------------------------
{
    static const int MAX_TIME_MS = 10000;
    wxProgressDialog progressDialog( wxT( "Scanning For Drivers, Interfaces and Devices" ),
                                     wxT( "Scanning for drivers, interfaces and devices...\n\nThis dialog will disappear automatically once this operation completes!" ),
                                     MAX_TIME_MS, // range
                                     pParent,
                                     wxPD_AUTO_HIDE | wxPD_APP_MODAL | wxPD_ELAPSED_TIME );
    UpdateDeviceListThread thread( devMgr );
    RunThreadAndShowProgress( &thread, &progressDialog, 100, MAX_TIME_MS );
}
