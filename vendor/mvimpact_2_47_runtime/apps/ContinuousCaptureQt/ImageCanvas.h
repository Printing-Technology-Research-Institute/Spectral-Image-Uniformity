//-----------------------------------------------------------------------------
#ifndef ImageCanvasH
#define ImageCanvasH ImageCanvasH
//-----------------------------------------------------------------------------
#include <mvIMPACT_CPP/mvIMPACT_acquire.h>

#include <apps/Common/qtIncludePrologue.h>
#include <QLabel>
#include <QMutex>
#include <apps/Common/qtIncludeEpilogue.h>

//-----------------------------------------------------------------------------
class ImageCanvas
//-----------------------------------------------------------------------------
{
    struct ImageCanvasImpl* pImpl_;
public:
    explicit ImageCanvas( QLabel* pLabel );
    void SetImage( Request* pRequest );
private:
    QMutex lock_;
};

#endif // ImageCanvas
