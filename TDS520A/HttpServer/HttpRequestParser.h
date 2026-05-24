// HttpServer/HttpRequestParser.h - Minimal HTTP/1.1 request parser
#pragma once
#include "pch.h"

// ni488.h (via Decl-32.h) defines GET as a GPIB command byte macro; undefine it before our enum
#ifdef GET
#undef GET
#endif

enum class HttpMethod { GET, POST, PUT, HTTP_DELETE, OPTIONS, HEAD, UNKNOWN };

struct HttpRequest
{
    HttpMethod  method{ HttpMethod::UNKNOWN };
    std::string path;
    std::string query;   // Raw query string (after '?')
    std::map<std::string, std::string> headers;
    std::string body;
    bool        isWebSocketUpgrade{ false };
    std::string wsKey;   // Sec-WebSocket-Key value

    bool IsValid() const { return method != HttpMethod::UNKNOWN && !path.empty(); }
};

struct HttpResponse
{
    int         statusCode{ 200 };
    std::string statusText{ "OK" };
    std::map<std::string, std::string> headers;
    std::string body;

    void SetContentType(const std::string& ct) { headers["Content-Type"] = ct; }
    void SetBody(const std::string& b, const std::string& ct = "text/plain")
    {
        body = b;
        SetContentType(ct);
        headers["Content-Length"] = std::to_string(b.size());
    }

    std::string Serialize() const;
};

class HttpRequestParser
{
public:
    // Returns true if a complete request was parsed.
    // Leaves partial data in internal buffer.
    bool Feed(const char* data, int len);
    bool HasRequest() const { return m_ready; }
    HttpRequest  TakeRequest();
    void Reset();

private:
    std::string  m_buf;
    bool         m_ready{ false };
    HttpRequest  m_current;

    bool ParseHeaders(const std::string& header_block);
    static HttpMethod ParseMethod(const std::string& m);
    static std::string ToLower(std::string s);
};
