#include "harmonicmarkdown.h"

#include <QRegularExpression>
#include <QStringList>

namespace {

QString applyInlineMarkdown(const QString &text)
{
    QStringList parts;
    QStringList codeParts = text.split(QLatin1Char('`'));

    for (int i = 0; i < codeParts.size(); ++i) {
        QString part = codeParts.at(i).toHtmlEscaped();
        if (i % 2 == 1) {
            parts.append(QStringLiteral("<code style=\"background-color: #2d2d2d; padding: 2px 4px; border-radius: 2px;\">%1</code>").arg(part));
            continue;
        }

        part.replace(QRegularExpression(QStringLiteral(R"(\*\*(.+?)\*\*)")), QStringLiteral("<b>\\1</b>"));
        part.replace(QRegularExpression(QStringLiteral(R"(__(.+?)__)")), QStringLiteral("<b>\\1</b>"));
        part.replace(QRegularExpression(QStringLiteral(R"((?<!\*)\*(?!\s)(.+?)(?<!\s)\*(?!\*))")), QStringLiteral("<i>\\1</i>"));
        part.replace(QRegularExpression(QStringLiteral(R"((?<!_)_(?!\s)(.+?)(?<!\s)_(?!_))")), QStringLiteral("<i>\\1</i>"));
        parts.append(part);
    }

    return parts.join(QString());
}

QString renderParagraph(const QString &text)
{
    return QStringLiteral("<p>%1</p>").arg(applyInlineMarkdown(text));
}

QString renderCodeBlock(const QString &text)
{
    return QStringLiteral("<pre style=\"background-color: #1e1e1e; color: #d4d4d4; padding: 8px; border-radius: 4px;\">%1</pre>")
        .arg(text.toHtmlEscaped());
}

}

QString harmonicMarkdownToHtml(const QString &markdown)
{
    QString normalized = markdown;
    normalized.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    normalized.replace(QLatin1Char('\r'), QLatin1Char('\n'));
    const QStringList lines = normalized.split(QLatin1Char('\n'));

    QStringList html;
    QStringList codeLines;
    bool inCodeBlock = false;
    bool inUnorderedList = false;
    bool inOrderedList = false;

    auto closeLists = [&]() {
        if (inUnorderedList) {
            html.append(QStringLiteral("</ul>"));
            inUnorderedList = false;
        }
        if (inOrderedList) {
            html.append(QStringLiteral("</ol>"));
            inOrderedList = false;
        }
    };

    for (const QString &line : lines) {
        if (line.startsWith(QStringLiteral("```"))) {
            closeLists();
            if (inCodeBlock) {
                html.append(renderCodeBlock(codeLines.join(QLatin1Char('\n'))));
                codeLines.clear();
                inCodeBlock = false;
            } else {
                inCodeBlock = true;
            }
            continue;
        }

        if (inCodeBlock) {
            codeLines.append(line);
            continue;
        }

        const QString trimmed = line.trimmed();
        if (trimmed.isEmpty()) {
            closeLists();
            continue;
        }

        QRegularExpressionMatch headingMatch = QRegularExpression(QStringLiteral(R"(^(#{1,3})\s+(.+)$)")).match(trimmed);
        if (headingMatch.hasMatch()) {
            closeLists();
            const int level = headingMatch.captured(1).size() + 2;
            html.append(QStringLiteral("<h%1>%2</h%1>").arg(QString::number(level), applyInlineMarkdown(headingMatch.captured(2))));
            continue;
        }

        QRegularExpressionMatch unorderedMatch = QRegularExpression(QStringLiteral(R"(^[-*]\s+(.+)$)")).match(trimmed);
        if (unorderedMatch.hasMatch()) {
            if (inOrderedList) {
                html.append(QStringLiteral("</ol>"));
                inOrderedList = false;
            }
            if (!inUnorderedList) {
                html.append(QStringLiteral("<ul>"));
                inUnorderedList = true;
            }
            html.append(QStringLiteral("<li>%1</li>").arg(applyInlineMarkdown(unorderedMatch.captured(1))));
            continue;
        }

        QRegularExpressionMatch orderedMatch = QRegularExpression(QStringLiteral(R"(^\d+\.\s+(.+)$)")).match(trimmed);
        if (orderedMatch.hasMatch()) {
            if (inUnorderedList) {
                html.append(QStringLiteral("</ul>"));
                inUnorderedList = false;
            }
            if (!inOrderedList) {
                html.append(QStringLiteral("<ol>"));
                inOrderedList = true;
            }
            html.append(QStringLiteral("<li>%1</li>").arg(applyInlineMarkdown(orderedMatch.captured(1))));
            continue;
        }

        closeLists();
        html.append(renderParagraph(trimmed));
    }

    closeLists();
    if (inCodeBlock) {
        html.append(renderCodeBlock(codeLines.join(QLatin1Char('\n'))));
    }

    return html.join(QString());
}
