#ifndef CLIPITEM_H
#define CLIPITEM_H

#include <string>
using std::wstring;
using std::string;
#include <vector>
using std::vector;
#include <stdexcept>
#include <iostream>

struct ClipItem
{
    enum Type {
        TYPE_NONE,
        TYPE_TEXT,  // Unicode text
        TYPE_RTF,   // Enriched text (RTF)
        TYPE_IMAGE, // Binary image data
    } type;

    wstring text;
    string rtf;
    vector<unsigned char> image;

    ClipItem() : type(TYPE_NONE) {}
    ClipItem(wstring text) : type(TYPE_TEXT), text(text) {}
    ClipItem(string rtf)   : type(TYPE_RTF), rtf(rtf) {}
    ClipItem(vector<unsigned char> image) : type(TYPE_IMAGE), image(image) {}

    bool IsEmpty() const
    {
        switch (type)
        {
        case TYPE_TEXT:  return text.empty();
        case TYPE_RTF:   return rtf.empty();
        case TYPE_IMAGE: return image.empty();
        case TYPE_NONE:  return true;
        }

        throw std::logic_error("Invalid ClipItem type");
    }

    wstring ToString() const
    {
        switch (type)
        {
        case TYPE_TEXT:   return text;
        case TYPE_RTF :   return wstring(rtf.begin(), rtf.end());
        case TYPE_IMAGE:  return L"Image";
        case TYPE_NONE:   throw std::logic_error("ToString() to ClipItem TYPE_NONE unavaiable");
        }

        throw std::logic_error("Invalid ClipItem type");
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
        case TYPE_NONE:  return true;
        }

        throw std::logic_error("Invalid ClipItem type");
    }

};

inline std::wostream &operator<<(std::wostream &os, const ClipItem &item)
{
    os << item.ToString();
    return os;
}

#endif // CLIPITEM_H
