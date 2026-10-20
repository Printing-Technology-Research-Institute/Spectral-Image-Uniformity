#include "ContinuousCaptureQt.h"
#include <QtWidgets/QApplication>

//-----------------------------------------------------------------------------
int main( int argc, char* argv[] )
//-----------------------------------------------------------------------------
{
    QApplication app( argc, argv );
    ContinuousCaptureQt window;
    window.show();
    return app.exec();
}
