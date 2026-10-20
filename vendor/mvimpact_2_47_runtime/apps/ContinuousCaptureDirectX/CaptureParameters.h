//-----------------------------------------------------------------------------
#ifndef CaptureParametersH
#define CaptureParametersH CaptureParametersH
//-----------------------------------------------------------------------------
#include <windows.h>
#include <process.h>
#include <mvIMPACT_CPP/mvIMPACT_acquire.h>

//-----------------------------------------------------------------------------
class CaptureDisplay
//-----------------------------------------------------------------------------
{
public:
    virtual ~CaptureDisplay() {}
    virtual void SetImage( const ImageBuffer* pIB ) = 0;
    virtual void RemoveImage( void ) = 0;
};

//-----------------------------------------------------------------------------
class CaptureParameters
//-----------------------------------------------------------------------------
{
    Device* pDev_;
    Connector c_;
    FunctionInterface fi_;
    Statistics statistics_;
    HANDLE hThread_;
    volatile bool boThreadRunning_;
    bool boStatisticalOutputActive_;
    const int DESTINATION_WIDTH_;
    const int DESTINATION_HEIGHT_;
    CaptureDisplay* pDisplay_;
public:
    explicit CaptureParameters( Device* p );
    ~CaptureParameters();

    void attachDisplay( CaptureDisplay* p )
    {
        pDisplay_ = p;
    }
    CaptureDisplay* getDisplay( void )
    {
        return pDisplay_;
    }

    static unsigned int __stdcall startCaptureThread( void* pData );
    void startThread( void );
    void stopThread( void );
    bool isThreadRunning( void ) const
    {
        return boThreadRunning_;
    }
    void captureThread( void );

    void toggleStatisticalOutput( void )
    {
        boStatisticalOutputActive_ = !boStatisticalOutputActive_;
    }
    bool isStatisticalOutputActive( void ) const
    {
        return boStatisticalOutputActive_;
    }

    Device* device( void ) const
    {
        return pDev_;
    }
    FunctionInterface& functionInterface( void )
    {
        return fi_;
    }
    const Statistics& statistics( void ) const
    {
        return statistics_;
    }

    int availableVideoChannels( void ) const;
    int currentVideoChannel( void ) const;
    void selectNextVideoChannel( void );
};

void        DbOutput( const char* pszFormat, ... );
std::string BuildFormattedString( const char* pszFormat, ... );

#endif // CaptureParametersH
