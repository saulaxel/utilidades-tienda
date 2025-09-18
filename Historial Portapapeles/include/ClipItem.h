#ifndef CLIPITEM_H
#define CLIPITEM_H

#include <string>
using std::wstring;
using std::string;
#include <vector>
using std::vector;

struct ClipItem
{
    enum Type {
        TYPE_TEXT,  // Unicode text
        TYPE_RTF,   // Enriched text (RTF)
        TYPE_IMAGE, // Binary image data
    } type;
    wstring text;
    string rtf;
    vector<unsigned char> image;

    ClipItem(wstring text) : type(TYPE_TEXT), text(text) {}
    ClipItem(string rtf) : type(TYPE_RTF), rtf(rtf) {}
    ClipItem(vector<unsigned char> image) : type(TYPE_IMAGE), image(image) {}

    bool Empty() const
    {
        switch (type)
        {
        case TYPE_TEXT:  return text.empty();
        case TYPE_RTF:   return rtf.empty();
        case TYPE_IMAGE: return image.empty();
        }

        return true;
    }

    bool operator==(const ClipItem &other) const
    {
        if (type != other.type)
            return false;

        switch (type)
        {
        case TYPE_TEXT:  return text == other.text;
        case TYPE_RTF:   return rtf == other.rtf;
        case TYPE_IMAGE: return image == other.image;
        }

        return false;
    }
};

#endif // CLIPITEM_H
