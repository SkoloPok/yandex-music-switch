// Реализация минимального XML-парсера для Yandex Music download-info

#include "xml_parser.h"

DownloadInfo XmlParser::parseDownloadInfo(const std::string& xml) {
    DownloadInfo info;

    info.host   = extractTag(xml, "host");
    info.path   = extractTag(xml, "path");
    info.ts     = extractTag(xml, "ts");
    info.s      = extractTag(xml, "s");
    info.region = extractTag(xml, "region");

    return info;
}

std::string XmlParser::extractTag(const std::string& xml, const std::string& tagName) {
    // Ищем открывающий тег <tagName>
    std::string openTag  = "<" + tagName + ">";
    std::string closeTag = "</" + tagName + ">";

    std::size_t startPos = xml.find(openTag);
    if (startPos == std::string::npos) {
        return "";
    }

    // Сдвигаемся за конец открывающего тега
    startPos += openTag.size();

    // Ищем закрывающий тег </tagName>
    std::size_t endPos = xml.find(closeTag, startPos);
    if (endPos == std::string::npos) {
        return "";
    }

    return xml.substr(startPos, endPos - startPos);
}
