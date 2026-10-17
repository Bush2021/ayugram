// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Bush2021, 2026
#pragma once

#include <optional>
#include <vector>

namespace AyuFeatures::LinkPreviewRules {

struct Rule {
	QString from;
	QString to;

	friend inline bool operator==(const Rule &, const Rule &) = default;
};

struct ParseResult {
	std::vector<Rule> rules;
	int invalidLine = 0;
};

[[nodiscard]] std::vector<Rule> Defaults();
[[nodiscard]] std::optional<Rule> MakeRule(
	const QString &from,
	const QString &to);
[[nodiscard]] ParseResult Parse(const QString &text);
[[nodiscard]] QString Format(const std::vector<Rule> &rules);
[[nodiscard]] QString Apply(const std::vector<Rule> &rules, const QString &url);

} // namespace AyuFeatures::LinkPreviewRules
