// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Bush2021, 2026
#include "ayu/features/link_preview/link_preview_rules.h"

#include <QtCore/QRegularExpression>
#include <QtCore/QUrl>

namespace AyuFeatures::LinkPreviewRules {
namespace {

[[nodiscard]] bool IsWildcard(const QString &host) {
	return host.startsWith(u"*."_q);
}

[[nodiscard]] QString NormalizeHost(const QString &value) {
	const auto wildcard = IsWildcard(value);
	const auto host = wildcard ? value.mid(2) : value;
	if (!host.contains(u'.')
		|| host.startsWith(u'.')
		|| host.endsWith(u'.')) {
		return QString();
	}
	auto url = QUrl();
	url.setHost(host, QUrl::StrictMode);
	const auto normalized = url.isValid() ? url.host() : QString();
	if (normalized.isEmpty()) {
		return QString();
	}
	return wildcard ? (u"*."_q + normalized) : normalized;
}

[[nodiscard]] std::optional<QString> Subdomain(
		const QString &pattern,
		const QString &host) {
	if (!IsWildcard(pattern)) {
		return (host == pattern || host == (u"www."_q + pattern))
			? std::make_optional(QString())
			: std::nullopt;
	}
	const auto domain = pattern.mid(2);
	if (host == domain) {
		return QString();
	} else if (host.endsWith(u"."_q + domain)) {
		return host.left(host.size() - domain.size());
	}
	return std::nullopt;
}

} // namespace

std::vector<Rule> Defaults() {
	return {
		{ u"twitter.com"_q, u"fixupx.com"_q },
		{ u"x.com"_q, u"fixupx.com"_q },
		{ u"*.tiktok.com"_q, u"*.kktiktok.com"_q },
		{ u"reddit.com"_q, u"vxreddit.com"_q },
		{ u"instagram.com"_q, u"kkclip.com"_q },
		{ u"pixiv.net"_q, u"phixiv.net"_q },
	};
}

std::optional<Rule> MakeRule(const QString &from, const QString &to) {
	const auto normalizedFrom = NormalizeHost(from.trimmed());
	const auto normalizedTo = NormalizeHost(to.trimmed());
	if (normalizedFrom.isEmpty()
		|| normalizedTo.isEmpty()
		|| (IsWildcard(normalizedTo) && !IsWildcard(normalizedFrom))) {
		return std::nullopt;
	}
	return Rule{ normalizedFrom, normalizedTo };
}

ParseResult Parse(const QString &text) {
	static const auto kSpaces = QRegularExpression(u"\\s+"_q);
	auto result = ParseResult();
	const auto lines = text.split(u'\n');
	for (auto i = 0; i != int(lines.size()); ++i) {
		const auto parts = lines[i].split(kSpaces, Qt::SkipEmptyParts);
		if (parts.isEmpty()) {
			continue;
		}
		const auto rule = (parts.size() == 2)
			? MakeRule(parts[0], parts[1])
			: std::nullopt;
		if (!rule) {
			return { .invalidLine = i + 1 };
		}
		result.rules.push_back(*rule);
	}
	return result;
}

QString Format(const std::vector<Rule> &rules) {
	auto lines = QStringList();
	lines.reserve(int(rules.size()));
	for (const auto &rule : rules) {
		lines.push_back(rule.from + u" "_q + rule.to);
	}
	return lines.join(u'\n');
}

QString Apply(const std::vector<Rule> &rules, const QString &url) {
	auto parsed = QUrl(url);
	if (!parsed.isValid() || parsed.host().isEmpty()) {
		return url;
	}
	const auto host = parsed.host().toLower();
	for (const auto &rule : rules) {
		if (const auto subdomain = Subdomain(rule.from, host)) {
			parsed.setHost(IsWildcard(rule.to)
				? (*subdomain + rule.to.mid(2))
				: rule.to);
			return parsed.toString();
		}
	}
	return url;
}

} // namespace AyuFeatures::LinkPreviewRules
