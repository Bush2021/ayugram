// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Bush2021, 2026
#pragma once

struct TextWithEntities;
struct TextWithTags;

namespace AyuFeatures::TextSpacing {

[[nodiscard]] TextWithEntities ForDisplay(TextWithEntities text);
[[nodiscard]] TextWithTags ForSending(TextWithTags text);
[[nodiscard]] TextWithTags ForEditing(TextWithTags text);
[[nodiscard]] TextWithEntities ForEditing(TextWithEntities text);

} // namespace AyuFeatures::TextSpacing
