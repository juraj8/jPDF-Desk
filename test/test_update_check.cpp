#include "update_check.h"

#include <QDebug>
#include <QVersionNumber>

int main()
{
    const QVersionNumber current(1, 3, 9);
    const struct {
        const char *response;
        const char *expectedTag;
        bool invalid;
    } cases[] = {
        {R"({"tag_name":"v1.3.10","draft":false,"prerelease":false})", "v1.3.10", false},
        {R"({"tag_name":"v1.3.9"})", "", false},
        {R"({"tag_name":"v1.2.99"})", "", false},
        {R"({"tag_name":"v2.0.0-rc1","prerelease":true})", "", false},
        {R"({"tag_name":"v2.0.0","draft":true})", "", false},
        {R"({"tag_name":"v2.0.0-extra"})", "", true},
        {R"({"tag_name":"v999999999999.0.0"})", "", true},
        {R"({"message":"Not Found"})", "", true},
        {"[]", "", true},
        {"not JSON", "", true},
    };
    QString error;
    for (const auto &test : cases) {
        const auto tag = newerStableReleaseTag(test.response, current, error);
        if (tag != QString::fromLatin1(test.expectedTag) || test.invalid != !error.isEmpty()) {
            qCritical() << "Unexpected result for" << test.response << ":" << tag << error;
            return 1;
        }
    }
    return 0;
}
