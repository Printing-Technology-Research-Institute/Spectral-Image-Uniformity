#include "ImageCanvas.h"
#include "mvDisplay/Include/mvIMPACT_acquire_display.h"

//=============================================================================
//================= Implementation ImageCanvasImpl ============================
//=============================================================================
//-----------------------------------------------------------------------------
struct ImageCanvasImpl
//-----------------------------------------------------------------------------
{
    mvIMPACT::acquire::display::ImageDisplay* pImageDisplay_;
    HBRUSH hBackgroundBrush_;
    HWND handle_;

    explicit ImageCanvasImpl( QLabel* pLabel )
    {
        pImageDisplay_ = new mvIMPACT::acquire::display::ImageDisplay( reinterpret_cast<WindowHandle>( pLabel->winId() ) );
        pImageDisplay_->SetImage( ( Request* ) 0 );
        pImageDisplay_->SetDisplayMode( mvIMPACT::acquire::display::DM_Default );
        hBackgroundBrush_ = CreateHatchBrush( HS_DIAGCROSS, GetSysColor( COLOR_APPWORKSPACE ) );
        pImageDisplay_->SetBackgroundBrush( hBackgroundBrush_ );
    }
    ~ImageCanvasImpl()
    {
        delete pImageDisplay_;
        DeleteObject( hBackgroundBrush_ );
    }
};

//=============================================================================
//================= Implementation ImageCanvas ================================
//=============================================================================
//-----------------------------------------------------------------------------
ImageCanvas::ImageCanvas( QLabel* pLabel )
//-----------------------------------------------------------------------------
{
    pImpl_ = new ImageCanvasImpl( pLabel );
}

//-----------------------------------------------------------------------------
void ImageCanvas::SetImage( Request* pRequest )
//-----------------------------------------------------------------------------
{
    pImpl_->pImageDisplay_->SetImage( pRequest );
    pImpl_->pImageDisplay_->Update();
    if( pRequest )
    {
        pRequest->unlock();
    }
    // IMPORTANT: This approach only works if no repaint of the GUI is triggered! If your application shall be
    // able to deal with resize/repaint messages you need to keep the request which is currently attached
    // to the display locked until you assign another one to the display as unlocking might result in freeing
    // the attached memory!!!
    // - For a more sophisticated example of dealing with request objects and image
    //   analysis and display have a look at the source code of wxPropView!
    // - For a generic Qt way of displaying image data have a look at the 'ImageCanvas.cpp' file in the
    //   top-level folder of this application!
}
