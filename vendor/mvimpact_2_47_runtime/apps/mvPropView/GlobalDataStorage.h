//-----------------------------------------------------------------------------
#ifndef GlobalDataStorageH
#define GlobalDataStorageH GlobalDataStorageH
//-----------------------------------------------------------------------------
#include <map>
#include <mvIMPACT_CPP/mvIMPACT_acquire.h>

#include <apps/Common/wxIncludePrologue.h>
#include <wx/wx.h>
#include <wx/colour.h>
#include <apps/Common/wxIncludeEpilogue.h>
//-----------------------------------------------------------------------------
/// \brief This is a singleton class
class GlobalDataStorage
//-----------------------------------------------------------------------------
{
public:
    ~GlobalDataStorage();
    static GlobalDataStorage* Instance( void );
    mvIMPACT::acquire::TComponentVisibility GetComponentVisibility( void ) const
    {
        return componentVisibility_;
    }
    void SetComponentVisibility( mvIMPACT::acquire::TComponentVisibility componentVisibility )
    {
        componentVisibility_ = componentVisibility;
    }
    void SetListBackgroundColour( const wxColour& colour )
    {
        LIST_BACKGROUND_COLOUR_ = colour;
    }
    void ConfigureComponentVisibilitySupport( bool boIsSupported )
    {
        boComponentVisibilitySupported_ = boIsSupported;
    }
    bool IsComponentVisibilitySupported( void ) const
    {
        return boComponentVisibilitySupported_;
    }

    //-----------------------------------------------------------------------------
    enum TPropGridColour
    //-----------------------------------------------------------------------------
    {
        pgcInvisibleExpertFeature,
        pgcInvisibleFeature,
        pgcInvisibleGuruFeature,
        pgcListBackground,
        pgcPropertyText,
        pgcContentDescriptorText
    };
    const wxColour& GetPropGridColour( TPropGridColour colour ) const;

    //-----------------------------------------------------------------------------
    enum TBitmap
    //-----------------------------------------------------------------------------
    {
        bIcon_GreyPreset,
        bIcon_FactoryPreset,
        bIcon_Tacho,
        bIcon_Diamond,
        bIcon_Empty,
        bIcon_Warning
    };
    const wxBitmap* GetBitmap( TBitmap bitmapType ) const;
private:
    mvIMPACT::acquire::TComponentVisibility componentVisibility_;
    bool boComponentVisibilitySupported_;
    wxColour LIST_BACKGROUND_COLOUR_;
    const wxColour PROPERTY_TEXT_COLOUR_;
    const wxColour INVISIBLE_EXPERT_FEATURE_COLOUR_;
    const wxColour INVISIBLE_GURU_FEATURE_COLOUR_;
    const wxColour INVISIBLE_FEATURE_COLOUR_;
    std::map<TBitmap, wxBitmap*> bitmapHashTable_;
    static GlobalDataStorage* pInstance_;
    explicit GlobalDataStorage();

    GlobalDataStorage( const GlobalDataStorage& );
    GlobalDataStorage& operator=( const GlobalDataStorage& );
};

#endif // GlobalDataStorageH
