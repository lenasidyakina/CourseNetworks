#ifndef COURSE_HTTP_H
#define COURSE_HTTP_H

#include <string>

namespace http
{

    enum class eMethod
    {
        GET,
        POST,
        HEAD,
        PUT,
        UPDATE,
        DELETE
    };

    struct header
    {
        std::string path;
        eMethod method;
    };

    struct response
    {
        std::string code;
        std::string content_type;
        std::size_t content_length;
        std::string connection;
        std::string body;
    };

    const static response forbidden_403
            {
                    .code = "403 Forbidden",
                    .content_type = "text/plain",
                    .content_length = 26,
                    .connection = "close",
                    .body = "Forbidden: no permission."
            };

    const static response not_found_404
            {
                    .code = "404 Not Found",
                    .content_type = "text/plain",
                    .content_length = 16,
                    .connection = "close",
                    .body = "File not found."
            };

    static inline header parse_header(const std::string_view& data)
    {
        header result;
        size_t first_crlf = data.find("\r\n");
        if (first_crlf == std::string::npos)
        {
            // close_sock(thread_id, task.fd);
            return {};
        }
        std::string_view first_line = data.substr(0, first_crlf);

        std::string_view method;
        size_t space1 = first_line.find(' ');
        size_t space2 = first_line.find(' ', space1 + 1);
        if (space1 == std::string::npos || space2 == std::string::npos)
        {
            // close_sock(thread_id, task.fd);
            return {};
        }

        method = first_line.substr(0, space1);
        if (method == "GET")
            result.method = http::eMethod::GET;
        else
            result.method = http::eMethod::GET;
        result.path = first_line.substr(space1 + 1, space2 - space1 - 1);

        return result;
    }

}

#endif //COURSE_HTTP_H
