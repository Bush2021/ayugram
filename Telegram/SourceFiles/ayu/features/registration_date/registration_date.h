// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Bush2021, 2026
#pragma once

namespace AyuFeatures::RegistrationDate {

enum class Bound : uchar {
	Before,
	Around,
	After,
};

struct Estimate {
	Bound bound = Bound::Around;
	TimeId date = 0;
};

[[nodiscard]] Estimate EstimateForUser(uint64 userId);

} // namespace AyuFeatures::RegistrationDate
