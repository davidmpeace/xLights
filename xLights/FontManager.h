#ifndef FONTMANAGER_H
#define FONTMANAGER_H

#include <vector>
#include "wx/wx.h"

class xlFont
{
    public:
        xlFont(wxBitmap& bitmap_);
        virtual ~xlFont();
        wxBitmap* get_bitmap() { return &bitmap; }
        int GetWidth() { return char_width; }
        int GetHeight() { return char_height; }
        int GetCharWidth(int ascii) { return widths[ascii]; }
        int GetCapsHeight() { return caps_height; }
        void SetCapsHeight( int height ) { caps_height = height; }
        void GatherInfo();

    protected:
        int char_width;   // the standard character width
        int char_height;  // the standard character height
        int caps_height;  // the capital letter height
        int widths[128];  // the trimmed width of each character
        wxBitmap& bitmap;
};

class FontManager
{
    public:
        static FontManager& instance()
        {
            static FontManager me;
            return me;
        }

        void init();

        virtual ~FontManager();

        static wxArrayString get_font_names();
        static xlFont* get_font(wxString font_name);
        static int get_length(xlFont* font, wxString& text);

        enum XLIGHTS_FONTS {
            FNT_6_5x6_THIN_SYSTEM,
            FNT_6_5x6_THIN_VERTICAL_SYSTEM,
            FNT_6_6x6_THIN_SYSTEM,
            FNT_6_6x6_THIN_VERTICAL_SYSTEM,
            FNT_8_8x8_THIN_SYSTEM,
            FNT_8_8x8_THIN_VERTICAL_SYSTEM,
            FNT_10_12x12_BOLD_SYSTEM,
            FNT_10_12x12_BOLD_VERTICAL_SYSTEM,
            FNT_10_12x12_THIN_SYSTEM,
            FNT_10_12x12_THIN_VERTICAL_SYSTEM,
            FNT_12_15x15_BOLD_SYSTEM,
            FNT_12_15x15_BOLD_VERTICAL_SYSTEM,
            NUM_XL_FONTS
        };

    protected:

    private:
        FontManager();
        FontManager(FontManager const&);     // Don't implement
        void operator=(FontManager const&);  // Don't implement

        static std::vector<wxBitmap> bitmaps;
        static std::vector<xlFont> fonts;
        static bool initialized;
        static wxArrayString names;
};



#endif // FONTMANAGER_H
