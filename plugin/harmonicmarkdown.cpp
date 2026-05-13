#include "harmonicmarkdown.h"

#include <QRegularExpression>
#include <QStringList>

namespace {

QString applyInlineStyles(QString text)
{
    text = text.toHtmlEscaped();
    text.replace(QRegularExpression(QStringLiteral(R"(\*\*(.+?)\*\*)")), QStringLiteral("<b>\\1</b>"));
    text.replace(QRegularExpression(QStringLiteral(R"(__(.+?)__)")), QStringLiteral("<b>\\1</b>"));
    text.replace(QRegularExpression(QStringLiteral(R"((?<!\*)\*(?!\s)(.+?)(?<!\s)\*(?!\*))")), QStringLiteral("<i>\\1</i>"));
    text.replace(QRegularExpression(QStringLiteral(R"((?<!_)_(?!\s)(.+?)(?<!\s)_(?!_))")), QStringLiteral("<i>\\1</i>"));
    return text;
}

QString applyInlineMarkdown(const QString &text)
{
    static const QRegularExpression inlineCodePattern(QStringLiteral(R"(`([^`]+)`)"));

    QString html;
    int lastMatchEnd = 0;
    QRegularExpressionMatchIterator matches = inlineCodePattern.globalMatch(text);
    while (matches.hasNext()) {
        const QRegularExpressionMatch match = matches.next();
        html += applyInlineStyles(text.mid(lastMatchEnd, match.capturedStart() - lastMatchEnd));
        html += QStringLiteral("<code style=\"background-color: #2d2d2d; padding: 2px 4px; border-radius: 2px;\">%1</code>")
                    .arg(match.captured(1).toHtmlEscaped());
        lastMatchEnd = match.capturedEnd();
    }

    html += applyInlineStyles(text.mid(lastMatchEnd));
    return html;
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
            const int level = headingMatch.captured(1).size() + 2; // Offset headings to keep chat sidebar typography intentionally compact.
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
