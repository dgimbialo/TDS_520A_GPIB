// HttpServer/HttpRequestParser.cpp
#include "pch.h"
#include "HttpRequestParser.h"

static std::string ToLowerS(std::string s)
{
    for (auto& c : s) c = static_cast<char>(::tolower(c));
    return s;
}

bool HttpRequestParser::Feed(const char* data, int len)
{
    if (len <= 0) return false;
    m_buf.append(data, static_cast<size_t>(len));

    // Find end of headers
    auto hdrEnd = m_buf.find("\r\n\r\n");
    if (hdrEnd == std::string::npos) return false;

    std::string headerBlock = m_buf.substr(0, hdrEnd);
    if (!ParseHeaders(headerBlock)) return false;

    // Handle body (for POST)
    size_t bodyStart = hdrEnd + 4;
    size_t contentLength = 0;
    auto it = m_current.headers.find("content-length");
    if (it != m_current.headers.end())
    {
        try { contentLength = static_cast<size_t>(std::stoi(it->second)); }
        catch (...) {}
    }

    if (m_buf.size() < bodyStart + contentLength)
        return false;  // Need more data

    m_current.body = m_buf.substr(bodyStart, contentLength);
    m_buf = m_buf.substr(bodyStart + contentLength);  // Leftover for next request
    m_ready = true;
    return true;
}

bool HttpRequestParser::ParseHeaders(const std::string& block)
{
    // Split into lines
    std::vector<std::string> lines;
    size_t pos = 0;
    while (pos < block.size())
    {
        auto eol = block.find("\r\n", pos);
        if (eol == std::string::npos) eol = block.size();
        lines.push_back(block.substr(pos, eol - pos));
        pos = eol + 2;
    }

    if (lines.empty()) return false;

    // Request line: METHOD PATH HTTP/1.x
    auto& rl = lines[0];
    auto sp1 = rl.find(' ');
    if (sp1 == std::string::npos) return false;
    auto sp2 = rl.find(' ', sp1 + 1);
    if (sp2 == std::string::npos) return false;

    m_current.method = ParseMethod(rl.substr(0, sp1));
    if (m_current.method == HttpMethod::UNKNOWN) return false;

    std::string fullPath = rl.substr(sp1 + 1, sp2 - sp1 - 1);
    auto qpos = fullPath.find('?');
    if (qpos != std::string::npos)
    {
        m_current.path  = fullPath.substr(0, qpos);
        m_current.query = fullPath.substr(qpos + 1);
    }
    else
    {
        m_current.path = fullPath;
    }

    // Basic path validation: reject path traversal
    if (m_current.path.find("..") != std::string::npos)
    {
        m_current.method = HttpMethod::UNKNOWN;
        return false;
    }

    // Header fields
    for (size_t i = 1; i < lines.size(); ++i)
    {
        auto colon = lines[i].find(':');
        if (colon == std::string::npos) continue;
        std::string key = ToLowerS(lines[i].substr(0, colon));
        std::string val = lines[i].substr(colon + 1);
        // Trim leading space
        while (!val.empty() && val.front() == ' ') val.erase(val.begin());
        m_current.headers[key] = val;
    }

    // WebSocket detection
    auto upg = m_current.headers.find("upgrade");
    if (upg != m_current.headers.end() && ToLowerS(upg->second) == "websocket")
    {
        m_current.isWebSocketUpgrade = true;
        auto wsKey = m_current.headers.find("sec-websocket-key");
        if (wsKey != m_current.headers.end())
            m_current.wsKey = wsKey->second;
    }

    return true;
}

HttpMethod HttpRequestParser::ParseMethod(const std::string& m)
{
    if (m == "GET")     return HttpMethod::GET;
    if (m == "POST")    return HttpMethod::POST;
    if (m == "PUT")     return HttpMethod::PUT;
    if (m == "DELETE")  return HttpMethod::HTTP_DELETE;
    if (m == "OPTIONS") return HttpMethod::OPTIONS;
    if (m == "HEAD")    return HttpMethod::HEAD;
    return HttpMethod::UNKNOWN;
}

HttpRequest HttpRequestParser::TakeRequest()
{
    m_ready = false;
    return std::move(m_current);
}

void HttpRequestParser::Reset()
{
    m_buf.clear();
    m_ready = false;
    m_current = HttpRequest{};
}

std::string HttpResponse::Serialize() const
{
    std::string out;
    out.reserve(512 + body.size());
    out += "HTTP/1.1 ";
    out += std::to_string(statusCode);
    out += " ";
    out += statusText;
    out += "\r\n";
    for (const auto& h : headers)
    {
        out += h.first;
        out += ": ";
        out += h.second;
        out += "\r\n";
    }
    out += "\r\n";
    out += body;
    return out;
}
