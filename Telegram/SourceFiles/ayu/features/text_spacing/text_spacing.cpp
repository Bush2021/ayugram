// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Bush2021, 2026
#include "ayu/features/text_spacing/text_spacing.h"

#include "ayu/ayu_settings.h"
#include "ayu/ayu_text_spacing.h"
#include "ui/text/text_entity.h"

namespace AyuFeatures::TextSpacing {
namespace {

constexpr auto kParseFlags = TextParseLinks
	| TextParseMentions
	| TextParseHashtags
	| TextParseBotCommands;

// Bots read their commands and arguments verbatim.
[[nodiscard]] bool IsCommand(const QString &text) {
	return text.startsWith(QChar('/'));
}

// Links typed as plain text get no entities until sending, find them here.
[[nodiscard]] std::vector<int> FindInsertions(TextWithEntities text) {
	TextUtilities::ParseEntities(text, kParseFlags);
	return AyuTextSpacing::FindInsertions(text);
}

[[nodiscard]] TextWithTags AddSpaces(const TextWithTags &text) {
	return AyuTextSpacing::AddSpaces(text, FindInsertions({
		text.text,
		TextUtilities::ConvertTextTagsToEntities(text.tags),
	}));
}

[[nodiscard]] TextWithEntities AddSpaces(const TextWithEntities &text) {
	return AyuTextSpacing::AddSpaces(text, FindInsertions(text));
}

} // namespace

TextWithEntities ForDisplay(TextWithEntities text) {
	if (!AyuSettings::getInstance().cjkSpacingInMessages()) {
		return text;
	}
	return AyuTextSpacing::AddSpaces(
		text,
		AyuTextSpacing::FindInsertions(text));
}

TextWithTags ForSending(TextWithTags text) {
	if (!AyuSettings::getInstance().cjkSpacingWhenSending()
		|| IsCommand(text.text)) {
		return text;
	}
	return AddSpaces(text);
}

TextWithTags ForEditing(TextWithTags text) {
	if (!AyuSettings::getInstance().cjkSpacingWhenEditing()
		|| IsCommand(text.text)) {
		return text;
	}
	return AddSpaces(text);
}

TextWithEntities ForEditing(TextWithEntities text) {
	if (!AyuSettings::getInstance().cjkSpacingWhenEditing()
		|| IsCommand(text.text)) {
		return text;
	}
	return AddSpaces(text);
}

} // namespace AyuFeatures::TextSpacing
