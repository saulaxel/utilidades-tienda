#ifndef CLIPITEM_H
#define CLIPITEM_H

#include <string>
using std::wstring;
using std::string;
#include <vector>
using std::vector;
#include <stdexcept>
#include <iostream>

#define CHARS_TO_SHOW 50

struct ClipItem
{
    enum Type {
        TYPE_TEXT,  // Unicode text
        TYPE_HTML,  // Enriched text (HTML)
        TYPE_FILE,  // List of files
        TYPE_IMAGE, // Binary image data
        TYPE_NONE,
    } type;

    string html;
    wstring text;
    vector<unsigned char> image;
    vector<unsigned char> file;

    ClipItem() : type(TYPE_NONE) {}
    ClipItem(wstring text) : type(TYPE_TEXT), text(text) {}
    ClipItem(string html, wstring text)   : type(TYPE_HTML), html(html), text(text) {}
    ClipItem(vector<unsigned char> image) : type(TYPE_IMAGE), image(image) {}
    ClipItem(vector<unsigned char> file, int) : type(TYPE_FILE), file(file) {}

    bool IsEmpty() const
    {
        switch (type)
        {
        case TYPE_TEXT:  return text.empty();
        // HTML never looks empty because of metadata, even if the actual content is
        // The way to check if empty is to see the actual text and not all content
        case TYPE_HTML:  return text.empty();
        case TYPE_IMAGE: return image.empty();
        case TYPE_FILE:  return file.empty();
        case TYPE_NONE:  return true;
        }

        throw std::logic_error("Invalid ClipItem type");
    }

    wstring ToString() const
    {
        wstring result;
        switch (type)
        {
        case TYPE_TEXT:   result = text;      break;
        case TYPE_HTML:   result = text;      break;
        case TYPE_IMAGE:  result = L"Imagen"; break;
        case TYPE_FILE:   result = text;      break;
        case TYPE_NONE:   result = L"Empty";  break;
        default: throw std::logic_error("Invalid ClipItem type");
        }

        // Limit length
        if (result.size() > CHARS_TO_SHOW)
        {
            result.resize(CHARS_TO_SHOW);
        }

        // Limit text to one line
        size_t pos = result.find(L'\n');
        if (pos != wstring::npos)
            result.erase(pos);

        return result;
    }

    const void *PtrTextData() const
    {
        if (type != TYPE_TEXT && type != TYPE_HTML)
            throw std::logic_error("Text data not available");
        return static_cast<const void *>(&text[0]);
    }

    const void *PtrHtmlData() const
    {
        if (type != TYPE_HTML)
            throw std::logic_error("Html data not available");
        return static_cast<const void *>(&html[0]);
    }

    const void *PtrImageData() const
    {
        if (type != TYPE_IMAGE)
            throw std::logic_error("Image data not available");
        return static_cast<const void *>(&image[0]);
    }

    const void *PtrFileData() const
    {
        if (type != TYPE_FILE)
            throw std::logic_error("File data not available");
        return static_cast<const void *>(&file[0]);
    }

    bool operator==(const ClipItem &other) const
    {
        if (type != other.type)
            return false;

        switch (type)
        {
        case TYPE_TEXT:  return text == other.text;
        case TYPE_HTML:  return html == other.html;
        case TYPE_IMAGE: return image == other.image;
        case TYPE_FILE : return file == other.file;
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
